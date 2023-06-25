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

#include <vector>
#include "shadowhook_util.h"
#include "utils/debug.h"
#include "second_party/byte_hook/shadowhook.h"

namespace bytedance {
    namespace atrace {
        namespace shadowhook {
            static std::vector<void *> stubs;

            bool hook_sym_name(const char *lib_name,
                               const char *sym_name,
                               void *new_addr) {
                return hook_sym_name(lib_name, sym_name, new_addr, nullptr);
            }

            bool hook_sym_name(const char *lib_name,
                               const char *sym_name,
                               void *new_addr,
                               void **origin_addr) {
#ifndef __LP64__
                ALOGE("shadow hook %s:%s failed. 32bit is disabled.", lib_name, sym_name);
                return false;
#endif
                void *stub = shadowhook_hook_sym_name(lib_name, sym_name, new_addr, origin_addr);
                if (stub == nullptr) {
                    ALOGE("shadow hook %s:%s failed:%s", lib_name, sym_name,
                          shadowhook_to_errmsg(shadowhook_get_errno()));
                } else {
                    stubs.push_back(stub);
                }
                return stub != nullptr;
            }

            void unhook_all() {
                for (auto const stub: stubs) {
                    if (stub != nullptr) {
                        shadowhook_unhook(stub);
                    }
                }
                stubs.clear();
            }
        }// namespace shadow_hook
    }  // namespace atrace
}  // namespace bytedance
