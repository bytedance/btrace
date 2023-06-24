/*
 * Copyright (C) 2021 ByteDance Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "second_party/byte_hook/shadowhook.h"

namespace bytedance {
    namespace atrace {
        namespace shadowhook {
            bool hook_sym_name(const char *lib_name,
                               const char *sym_name,
                               void *new_addr);

            bool hook_sym_name(const char *lib_name,
                               const char *sym_name,
                               void *new_addr,
                               void **origin_addr);

            void unhook_all();
        }// namespace shadow_hook
    }  // namespace atrace
}  // namespace bytedance
