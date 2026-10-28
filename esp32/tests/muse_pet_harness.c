/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Drives muse_pet.c for test_muse_pet.py. Commands (one per line):
 *   t <us>       set the monotonic clock (esp_timer_get_time)
 *   w <epoch>    set the wall clock (time()), 0 = invalid/not-synced
 *   sleep <0|1>  set muse_state_asleep()
 *   init         call muse_pet_init()
 *   tick         call muse_pet_tick()
 *   feed         call muse_pet_feed()
 *   pet          call muse_pet_pet()
 *   s            print stats as JSON: {"hunger":H,"energy":E,"mood":M}
 *   nwrites      print how many NVS commits have happened since last reset
 *   reset_writes reset the NVS commit counter
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Pull in the fakes' type definitions before we define the implementations. */
#include "esp_err.h"
#include "nvs.h"

/* ---- platform fakes ---- */

static int64_t s_clock_us;
static time_t  s_wall_time;
static bool    s_asleep;
static int     s_nvs_writes;

int64_t esp_timer_get_time(void) { return s_clock_us; }

/* Override time() before muse_pet.c is included. The harness intercepts
 * calls made by muse_pet.c since it is compiled as part of this TU. */
#define time(t) harness_time(t)
static time_t harness_time(time_t *t) { if (t) *t = s_wall_time; return s_wall_time; }

bool muse_state_asleep(void) { return s_asleep; }

/* ---- NVS fake ---- */

#define FAKE_NVS_KEYS 16
#define FAKE_NVS_VAL  8

typedef struct {
    char    key[16];
    uint8_t data[FAKE_NVS_VAL];
    size_t  len;
    int     used;
} fake_entry_t;

static fake_entry_t s_nvs_store[FAKE_NVS_KEYS];

static fake_entry_t *nvs_find(const char *key, int add)
{
    fake_entry_t *free_slot = NULL;
    for (int i = 0; i < FAKE_NVS_KEYS; i++) {
        if (s_nvs_store[i].used && !strcmp(s_nvs_store[i].key, key))
            return &s_nvs_store[i];
        if (!s_nvs_store[i].used && !free_slot)
            free_slot = &s_nvs_store[i];
    }
    if (!add || !free_slot) return NULL;
    strncpy(free_slot->key, key, sizeof(free_slot->key) - 1);
    free_slot->used = 1;
    return free_slot;
}

/* nvs_handle_t comes from pet_fakes/nvs.h (uint32_t); handle 1 = open. */
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *out)
{
    (void)name; (void)mode;
    *out = 1;
    return ESP_OK;
}
void      nvs_close(nvs_handle_t h) { (void)h; }
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; s_nvs_writes++; return ESP_OK; }

static esp_err_t get_raw(const char *key, void *out, size_t expect)
{
    fake_entry_t *e = nvs_find(key, 0);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    if (e->len != expect) return ESP_ERR_NVS_TYPE_MISMATCH;
    memcpy(out, e->data, expect);
    return ESP_OK;
}
static esp_err_t set_raw(const char *key, const void *val, size_t len)
{
    if (len > FAKE_NVS_VAL) return ESP_ERR_NVS_INVALID_LENGTH;
    fake_entry_t *e = nvs_find(key, 1);
    if (!e) return ESP_ERR_NVS_NO_FREE_PAGES;
    memcpy(e->data, val, len);
    e->len = len;
    return ESP_OK;
}

esp_err_t nvs_get_u8 (nvs_handle_t h, const char *k, uint8_t  *v) { (void)h; return get_raw(k,v,1); }
esp_err_t nvs_set_u8 (nvs_handle_t h, const char *k, uint8_t   v) { (void)h; return set_raw(k,&v,1); }
esp_err_t nvs_get_u32(nvs_handle_t h, const char *k, uint32_t *v) { (void)h; return get_raw(k,v,4); }
esp_err_t nvs_set_u32(nvs_handle_t h, const char *k, uint32_t  v) { (void)h; return set_raw(k,&v,4); }
esp_err_t nvs_erase_key(nvs_handle_t h, const char *k)
{
    (void)h;
    fake_entry_t *e = nvs_find(k, 0);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    memset(e, 0, sizeof(*e));
    return ESP_OK;
}

/* ---- include the unit under test ---- */
#include "muse_pet.c"

/* ---- main loop ---- */
int main(void)
{
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (!strncmp(line, "t ", 2)) {
            s_clock_us = strtoll(line + 2, NULL, 10);
        } else if (!strncmp(line, "w ", 2)) {
            s_wall_time = (time_t)strtol(line + 2, NULL, 10);
        } else if (!strncmp(line, "sleep ", 6)) {
            s_asleep = atoi(line + 6) != 0;
        } else if (!strcmp(line, "init")) {
            muse_pet_init();
        } else if (!strcmp(line, "tick")) {
            muse_pet_tick();
        } else if (!strcmp(line, "feed")) {
            muse_pet_feed();
        } else if (!strcmp(line, "pet")) {
            muse_pet_pet();
        } else if (!strcmp(line, "s")) {
            muse_pet_stats_t st = muse_pet_stats();
            printf("{\"hunger\":%.4f,\"energy\":%.4f,\"mood\":%.4f}\n",
                   (double)st.hunger, (double)st.energy, (double)st.mood);
        } else if (!strcmp(line, "nwrites")) {
            printf("%d\n", s_nvs_writes);
        } else if (!strcmp(line, "reset_writes")) {
            s_nvs_writes = 0;
        } else {
            fprintf(stderr, "bad command: %s\n", line);
            return 2;
        }
        fflush(stdout);
    }
    return 0;
}
