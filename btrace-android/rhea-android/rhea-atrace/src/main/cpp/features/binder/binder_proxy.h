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

#include <stdlib.h>
#include <string>
#include <list>

typedef int32_t status_t;

namespace bytedance {
    namespace atrace {
        status_t
        Parcel_writeInterfaceToken_proxy_12(void *Parcel, const char16_t *interface, size_t len);

        status_t Parcel_writeInterfaceToken_proxy(void *Parcel, const void *interface);

        status_t IPCThreadState_transact_proxy(void *IPCThreadState,
                                               int32_t handle,
                                               uint32_t code,
                                               const void *data,
                                               void *reply,
                                               uint32_t flags);

        std::list<std::string> all_interface_token_names();
    }  // namespace atrace
}  // namespace bytedance
