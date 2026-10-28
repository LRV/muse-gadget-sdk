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

/* Minimal NVS stub for the simulator. Backed by sim_nvs.c (in-memory only,
 * no persistence — sufficient for UI testing). */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_NVS_BASE            0x1100
#define ESP_ERR_NVS_NOT_INITIALIZED (ESP_ERR_NVS_BASE + 0x01)
#define ESP_ERR_NVS_NOT_FOUND       (ESP_ERR_NVS_BASE + 0x02)
#define ESP_ERR_NVS_TYPE_MISMATCH   (ESP_ERR_NVS_BASE + 0x03)
#define ESP_ERR_NVS_READ_ONLY       (ESP_ERR_NVS_BASE + 0x04)
#define ESP_ERR_NVS_INVALID_NAME    (ESP_ERR_NVS_BASE + 0x05)
#define ESP_ERR_NVS_NO_FREE_PAGES   (ESP_ERR_NVS_BASE + 0x06)
#define ESP_ERR_NVS_INVALID_LENGTH  (ESP_ERR_NVS_BASE + 0x07)

typedef uint32_t nvs_handle_t;

typedef enum {
    NVS_READONLY  = 0,
    NVS_READWRITE = 1,
} nvs_open_mode_t;

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *out);
void      nvs_close(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);

esp_err_t nvs_get_u8 (nvs_handle_t h, const char *key, uint8_t  *out);
esp_err_t nvs_set_u8 (nvs_handle_t h, const char *key, uint8_t   val);
esp_err_t nvs_get_u16(nvs_handle_t h, const char *key, uint16_t *out);
esp_err_t nvs_set_u16(nvs_handle_t h, const char *key, uint16_t  val);
esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *out);
esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t  val);
esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *out, size_t *len);
esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *val);
esp_err_t nvs_erase_key(nvs_handle_t h, const char *key);

#ifdef __cplusplus
}
#endif
