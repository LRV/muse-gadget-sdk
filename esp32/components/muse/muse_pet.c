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

#define _POSIX_C_SOURCE 200809L

#include "muse_pet.h"

#include <time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "muse_state.h"
#include "nvs.h"

static const char *TAG = "muse_pet";
#define NS "pet"

/* --- Decay/restore rates (fraction of full range per second) ---
 * "A few points per hour" keeps the pet easy to maintain while still
 * reacting meaningfully to care. Adjust these to taste. */
#define HUNGER_DECAY_PER_S    (2.0f / (100.0f * 3600.0f))  /* 2 pts/hr */
#define ENERGY_DECAY_PER_S    (1.5f / (100.0f * 3600.0f))  /* 1.5 pts/hr */
#define MOOD_DECAY_PER_S      (1.2f / (100.0f * 3600.0f))  /* 1.2 pts/hr */
#define ENERGY_RESTORE_PER_S  (4.0f / (100.0f * 3600.0f))  /* 4 pts/hr while asleep */

/* Boost per care action (fraction of full range). */
#define FEED_BOOST  0.25f   /* +25 hunger */
#define PET_BOOST   0.15f   /* +15 mood */

/* Write NVS at most once per 3 minutes, plus on every explicit action. */
#define WRITE_INTERVAL_US  (3LL * 60LL * 1000000LL)

/* Minimum Unix timestamp considered valid (2020-01-01 00:00:00 UTC).
 * Values at or below this mean SNTP has not synced yet. */
#define MIN_VALID_EPOCH  1577836800UL

static float s_hunger = 1.0f;
static float s_energy = 1.0f;
static float s_mood   = 1.0f;

static int64_t s_last_tick_us;
static int64_t s_last_write_us;

static nvs_handle_t s_nvs;

static float clamped(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static void commit_stats(void)
{
    if (!s_nvs) {
        return;
    }
    nvs_set_u8(s_nvs, "hunger", (uint8_t)(s_hunger * 100.0f + 0.5f));
    nvs_set_u8(s_nvs, "energy", (uint8_t)(s_energy * 100.0f + 0.5f));
    nvs_set_u8(s_nvs, "mood",   (uint8_t)(s_mood   * 100.0f + 0.5f));
    time_t wall = time(NULL);
    if ((uint32_t)wall > MIN_VALID_EPOCH) {
        nvs_set_u32(s_nvs, "saved_ts", (uint32_t)wall);
    }
    nvs_commit(s_nvs);
    s_last_write_us = esp_timer_get_time();
}

void muse_pet_init(void)
{
    s_hunger = 1.0f;
    s_energy = 1.0f;
    s_mood   = 1.0f;

    if (nvs_open(NS, NVS_READWRITE, &s_nvs) != ESP_OK) {
        s_nvs = 0;
        ESP_LOGW(TAG, "NVS unavailable, stats not persisted");
    } else {
        uint8_t hv = 100, ev = 100, mv = 100;
        uint32_t ts = 0;
        nvs_get_u8(s_nvs,  "hunger",   &hv);
        nvs_get_u8(s_nvs,  "energy",   &ev);
        nvs_get_u8(s_nvs,  "mood",     &mv);
        nvs_get_u32(s_nvs, "saved_ts", &ts);

        s_hunger = clamped(hv / 100.0f);
        s_energy = clamped(ev / 100.0f);
        s_mood   = clamped(mv / 100.0f);

        /* Apply decay for time the device was powered off, if the wall clock
         * is valid (SNTP has run). Without valid time, resume from saved values. */
        if (ts > 0 && ts > MIN_VALID_EPOCH) {
            time_t wall = time(NULL);
            if ((uint32_t)wall > ts) {
                float elapsed = (float)((uint32_t)wall - ts);
                s_hunger = clamped(s_hunger - HUNGER_DECAY_PER_S * elapsed);
                s_energy = clamped(s_energy - ENERGY_DECAY_PER_S * elapsed);
                s_mood   = clamped(s_mood   - MOOD_DECAY_PER_S   * elapsed);
                ESP_LOGI(TAG, "offline %.0fs: applied decay", elapsed);
            }
        }
    }

    int64_t now = esp_timer_get_time();
    s_last_tick_us  = now;
    s_last_write_us = now;
    ESP_LOGI(TAG, "hunger %.0f energy %.0f mood %.0f",
             s_hunger * 100.0f, s_energy * 100.0f, s_mood * 100.0f);
}

void muse_pet_tick(void)
{
    int64_t now = esp_timer_get_time();
    float dt = (float)(now - s_last_tick_us) / 1e6f;
    s_last_tick_us = now;

    s_hunger = clamped(s_hunger - HUNGER_DECAY_PER_S * dt);
    s_mood   = clamped(s_mood   - MOOD_DECAY_PER_S   * dt);
    if (muse_state_asleep()) {
        s_energy = clamped(s_energy + ENERGY_RESTORE_PER_S * dt);
    } else {
        s_energy = clamped(s_energy - ENERGY_DECAY_PER_S * dt);
    }

    if (now - s_last_write_us >= WRITE_INTERVAL_US) {
        commit_stats();
    }
}

muse_pet_stats_t muse_pet_stats(void)
{
    return (muse_pet_stats_t){
        .hunger = s_hunger,
        .energy = s_energy,
        .mood   = s_mood,
    };
}

void muse_pet_feed(void)
{
    s_hunger = clamped(s_hunger + FEED_BOOST);
    muse_state_make_happy();
    commit_stats();
}

void muse_pet_pet(void)
{
    s_mood = clamped(s_mood + PET_BOOST);
    muse_state_make_happy();
    commit_stats();
}
