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

#include <stdbool.h>
#include "cJSON.h"

/* True when `command` is a pet.* command this module handles. */
bool muse_pet_cmd_handles(const char *command);

/*
 * Dispatch a pet.* Link command.  Returns a heap-allocated cJSON result
 * that the caller must free, or NULL on OOM.  Only call after checking
 * muse_pet_cmd_handles().
 */
cJSON *muse_pet_cmd_handle(const char *command);
