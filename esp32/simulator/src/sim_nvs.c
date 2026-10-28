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

/* In-memory NVS stub for the simulator.
 * Supports up to SIM_NVS_NS_MAX namespaces, each holding up to SIM_NVS_KEY_MAX
 * keys. No persistence — values live only for the duration of the process. */

#include "nvs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SIM_NVS_NS_MAX   8
#define SIM_NVS_KEY_MAX  16
#define SIM_NVS_VAL_MAX  64   /* bytes per value */

typedef struct {
    char  key[16];
    uint8_t data[SIM_NVS_VAL_MAX];
    size_t  len;
    int     used;
} sim_nvs_entry_t;

typedef struct {
    char            ns[16];
    sim_nvs_entry_t entries[SIM_NVS_KEY_MAX];
    int             used;
} sim_nvs_ns_t;

static sim_nvs_ns_t s_store[SIM_NVS_NS_MAX];

static sim_nvs_ns_t *ns_find_or_create(const char *name)
{
    sim_nvs_ns_t *free_slot = NULL;
    for (int i = 0; i < SIM_NVS_NS_MAX; i++) {
        if (s_store[i].used && !strcmp(s_store[i].ns, name)) {
            return &s_store[i];
        }
        if (!s_store[i].used && !free_slot) {
            free_slot = &s_store[i];
        }
    }
    if (!free_slot) {
        return NULL;
    }
    strncpy(free_slot->ns, name, sizeof(free_slot->ns) - 1);
    free_slot->ns[sizeof(free_slot->ns) - 1] = '\0';
    free_slot->used = 1;
    return free_slot;
}

static sim_nvs_entry_t *entry_find(sim_nvs_ns_t *ns, const char *key, int add)
{
    sim_nvs_entry_t *free_slot = NULL;
    for (int i = 0; i < SIM_NVS_KEY_MAX; i++) {
        if (ns->entries[i].used && !strcmp(ns->entries[i].key, key)) {
            return &ns->entries[i];
        }
        if (!ns->entries[i].used && !free_slot) {
            free_slot = &ns->entries[i];
        }
    }
    if (!add || !free_slot) {
        return NULL;
    }
    strncpy(free_slot->key, key, sizeof(free_slot->key) - 1);
    free_slot->key[sizeof(free_slot->key) - 1] = '\0';
    free_slot->used = 1;
    return free_slot;
}

/* handle is a 1-based index into s_store */
static sim_nvs_ns_t *ns_from_handle(nvs_handle_t h)
{
    if (h == 0 || (int)h > SIM_NVS_NS_MAX) {
        return NULL;
    }
    return &s_store[h - 1];
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *out)
{
    (void)mode;
    sim_nvs_ns_t *ns = ns_find_or_create(name);
    if (!ns) {
        return ESP_ERR_NVS_NO_FREE_PAGES;
    }
    *out = (nvs_handle_t)(ns - s_store + 1);
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle)
{
    (void)handle;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

static esp_err_t get_raw(nvs_handle_t h, const char *key, void *out, size_t expect)
{
    sim_nvs_ns_t *ns = ns_from_handle(h);
    if (!ns) return ESP_ERR_NVS_NOT_INITIALIZED;
    sim_nvs_entry_t *e = entry_find(ns, key, 0);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    if (e->len != expect) return ESP_ERR_NVS_TYPE_MISMATCH;
    memcpy(out, e->data, expect);
    return ESP_OK;
}

static esp_err_t set_raw(nvs_handle_t h, const char *key, const void *val, size_t len)
{
    sim_nvs_ns_t *ns = ns_from_handle(h);
    if (!ns) return ESP_ERR_NVS_NOT_INITIALIZED;
    if (len > SIM_NVS_VAL_MAX) return ESP_ERR_NVS_INVALID_LENGTH;
    sim_nvs_entry_t *e = entry_find(ns, key, 1);
    if (!e) return ESP_ERR_NVS_NO_FREE_PAGES;
    memcpy(e->data, val, len);
    e->len = len;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *out)
{
    return get_raw(h, key, out, sizeof(*out));
}
esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t val)
{
    return set_raw(h, key, &val, sizeof(val));
}
esp_err_t nvs_get_u16(nvs_handle_t h, const char *key, uint16_t *out)
{
    return get_raw(h, key, out, sizeof(*out));
}
esp_err_t nvs_set_u16(nvs_handle_t h, const char *key, uint16_t val)
{
    return set_raw(h, key, &val, sizeof(val));
}
esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *out)
{
    return get_raw(h, key, out, sizeof(*out));
}
esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t val)
{
    return set_raw(h, key, &val, sizeof(val));
}

esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *out, size_t *len)
{
    sim_nvs_ns_t *ns = ns_from_handle(h);
    if (!ns) return ESP_ERR_NVS_NOT_INITIALIZED;
    sim_nvs_entry_t *e = entry_find(ns, key, 0);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    if (!out) {
        *len = e->len;
        return ESP_OK;
    }
    if (*len < e->len) return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(out, e->data, e->len);
    *len = e->len;
    return ESP_OK;
}

esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *val)
{
    size_t len = strlen(val) + 1;
    return set_raw(h, key, val, len);
}

esp_err_t nvs_erase_key(nvs_handle_t h, const char *key)
{
    sim_nvs_ns_t *ns = ns_from_handle(h);
    if (!ns) return ESP_ERR_NVS_NOT_INITIALIZED;
    sim_nvs_entry_t *e = entry_find(ns, key, 0);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    memset(e, 0, sizeof(*e));
    return ESP_OK;
}
