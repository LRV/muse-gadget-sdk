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

#include "lvgl.h"
#include "muse_pet.h"

/*
 * Build the pet stat HUD and attach it to `face` (the main face tile).
 * `big_y` is the avatar's vertical offset from screen centre (s_big_y in
 * muse_ui.c).  Only call this on the full layout (not s_small).
 */
void muse_pet_ui_build(lv_obj_t *face, int big_y);

/*
 * Refresh the HUD from the current pet stats.  Call once per LVGL frame,
 * after muse_pet_tick().  No-op until muse_pet_ui_build() has run.
 */
void muse_pet_ui_update(void);
