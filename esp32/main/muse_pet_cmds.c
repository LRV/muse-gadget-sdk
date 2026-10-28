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

#include "muse_pet_cmds.h"

#include <string.h>

#include "cJSON.h"
#include "muse_pet.h"

bool muse_pet_cmd_handles(const char *command)
{
    return strncmp(command, "pet.", 4) == 0;
}

static cJSON *stats_payload(void)
{
    muse_pet_stats_t st = muse_pet_stats();
    cJSON *p = cJSON_CreateObject();
    if (!p) return NULL;
    cJSON_AddNumberToObject(p, "hunger", (int)(st.hunger * 100.0f + 0.5f));
    cJSON_AddNumberToObject(p, "energy", (int)(st.energy * 100.0f + 0.5f));
    cJSON_AddNumberToObject(p, "mood",   (int)(st.mood   * 100.0f + 0.5f));
    return p;
}

static cJSON *ok_with_stats(void)
{
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;
    cJSON_AddBoolToObject(result, "ok", true);
    cJSON *payload = stats_payload();
    if (!payload) { cJSON_Delete(result); return NULL; }
    cJSON_AddItemToObject(result, "payload", payload);
    return result;
}

cJSON *muse_pet_cmd_handle(const char *command)
{
    if (strcmp(command, "pet.feed") == 0) {
        muse_pet_feed();
        return ok_with_stats();
    }
    if (strcmp(command, "pet.pet") == 0) {
        muse_pet_pet();
        return ok_with_stats();
    }
    if (strcmp(command, "pet.status") == 0) {
        return ok_with_stats();
    }
    /* Unknown pet.* command — should not be reached if registration is correct. */
    cJSON *result = cJSON_CreateObject();
    if (result) {
        cJSON_AddBoolToObject(result, "ok", false);
        cJSON_AddStringToObject(result, "message", "unsupported pet command");
    }
    return result;
}
