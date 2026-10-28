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

#pragma once

typedef struct {
    float hunger;  /* 0..1, 1 = full */
    float energy;  /* 0..1, 1 = full */
    float mood;    /* 0..1, 1 = happy */
} muse_pet_stats_t;

/* Load stats from NVS and apply offline decay if wall clock is valid.
 * Call once after NVS is ready (after muse_settings_init). */
void muse_pet_init(void);

/* Advance decay by the time since the last call. Call every UI frame. */
void muse_pet_tick(void);

/* Read the current stats. */
muse_pet_stats_t muse_pet_stats(void);

/* Raise hunger and persist immediately. */
void muse_pet_feed(void);

/* Raise mood and persist immediately. */
void muse_pet_pet(void);
