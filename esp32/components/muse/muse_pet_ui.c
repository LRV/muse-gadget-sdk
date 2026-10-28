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

#include "muse_pet_ui.h"

#include "lvgl.h"
#include "muse_pet.h"

/* Three rows: letter icon + pill track + pill fill. */
#define PET_BAR_W 40
#define PET_BAR_H  5
#define PET_ROW_H 18   /* icon (7 px) + gap (2 px) + bar (5 px) + row gap (4 px) */

/* Colours reused from muse_ui.c's palette. */
#define COLOR_METER_OFF 0x1d1733
#define COLOR_ACCENT    0xa77dff
#define COLOR_DIM       0x8b84a8

/* Action button dimensions (right flank, mirroring the stat column). */
#define BTN_W  44
#define BTN_H  22
#define BTN_GAP 6

static lv_obj_t *s_container;
static lv_obj_t *s_bars[3];

static void on_feed(lv_event_t *e)
{
    (void)e;
    muse_pet_feed();
}

static void on_pet_action(lv_event_t *e)
{
    (void)e;
    muse_pet_pet();
}

static lv_obj_t *make_action_btn(lv_obj_t *parent, const char *label, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1535), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_DIM), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2555), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_ACCENT), 0);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);
    return btn;
}

void muse_pet_ui_build(lv_obj_t *face, int big_y)
{
    static const char * const icons[3] = { "H", "E", "M" };

    s_container = lv_obj_create(face);
    lv_obj_remove_style_all(s_container);
    lv_obj_set_size(s_container, 12 + PET_BAR_W, 3 * PET_ROW_H - 4);
    lv_obj_remove_flag(s_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(s_container, LV_ALIGN_LEFT_MID, 8, big_y);

    for (int i = 0; i < 3; i++) {
        lv_obj_t *lbl = lv_label_create(s_container);
        lv_obj_set_style_text_font(lbl, &lv_font_unscii_8, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x6060a0), 0);
        lv_label_set_text(lbl, icons[i]);
        lv_obj_set_pos(lbl, 0, i * PET_ROW_H + 1);

        lv_obj_t *track = lv_obj_create(s_container);
        lv_obj_remove_style_all(track);
        lv_obj_set_size(track, PET_BAR_W, PET_BAR_H);
        lv_obj_set_style_bg_color(track, lv_color_hex(COLOR_METER_OFF), 0);
        lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(track, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_pos(track, 12, i * PET_ROW_H + 9);

        s_bars[i] = lv_obj_create(s_container);
        lv_obj_remove_style_all(s_bars[i]);
        lv_obj_set_size(s_bars[i], PET_BAR_W, PET_BAR_H);
        lv_obj_set_style_bg_color(s_bars[i], lv_color_hex(0x60c070), 0);
        lv_obj_set_style_bg_opa(s_bars[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_bars[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_pos(s_bars[i], 12, i * PET_ROW_H + 9);
    }

    /* Action buttons — right flank, same vertical centre as the stat column. */
    lv_obj_t *btn_col = lv_obj_create(face);
    lv_obj_remove_style_all(btn_col);
    lv_obj_set_size(btn_col, BTN_W, 2 * BTN_H + BTN_GAP);
    lv_obj_remove_flag(btn_col, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(btn_col, LV_ALIGN_RIGHT_MID, -14, big_y);

    lv_obj_t *feed = make_action_btn(btn_col, "FEED", on_feed);
    lv_obj_set_pos(feed, 0, 0);

    lv_obj_t *pet = make_action_btn(btn_col, "PET", on_pet_action);
    lv_obj_set_pos(pet, 0, BTN_H + BTN_GAP);
}

void muse_pet_ui_update(void)
{
    if (!s_container) return;

    static int   s_last_w[3] = {-1, -1, -1};
    static float s_last_v[3] = {-1.0f, -1.0f, -1.0f};
    static const uint32_t tier_colors[3] = { 0xd04040, 0xd0a020, 0x60c070 };

    muse_pet_stats_t st = muse_pet_stats();
    const float vals[3] = { st.hunger, st.energy, st.mood };

    for (int i = 0; i < 3; i++) {
        float v = vals[i];
        int w = (int)(v * PET_BAR_W + 0.5f);
        if (w < 1) w = 1;

        int tier      = v >= 0.6f ? 2 : v >= 0.3f ? 1 : 0;
        int last_tier = s_last_v[i] >= 0.6f ? 2 : s_last_v[i] >= 0.3f ? 1 : 0;

        if (w != s_last_w[i]) {
            lv_obj_set_width(s_bars[i], w);
            s_last_w[i] = w;
        }
        if (tier != last_tier) {
            lv_obj_set_style_bg_color(s_bars[i], lv_color_hex(tier_colors[tier]), 0);
            s_last_v[i] = v;
        }
    }
}
