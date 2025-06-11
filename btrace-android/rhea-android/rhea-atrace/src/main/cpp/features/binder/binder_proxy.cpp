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

#define LOG_TAG "Rhea.binder_proxy"

#include <debug.h>
#include "binder_proxy.h"
#include "bytehook.h"
#include <string>
#include <locale>
#include <codecvt>
#include <unistd.h>
#include <trace.h>
#include <xcc_fmt.h>
#include <unordered_set>
#include <atomic>

namespace bytedance {
    namespace atrace {

        std::unordered_set<std::string> tokens;
        std::atomic_flag tokens_write_flag = ATOMIC_FLAG_INIT;

        // ref https://cs.android.com/android/platform/superproject/+/master:system/core/libutils/include/utils/String16.h;drc=master;l=39
        /*
        * Copyright (C) 2005 The Android Open Source Project
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
        class String16 {
        public:
            char16_t *mString;
        };

        class ParcelToken {
        public:
            uintptr_t parcel{};
            std::string token;
        };

        thread_local ParcelToken parcelToken;

        // interfaceToken is encoding in char16_t*. we should convert to char*
        static std::string u16string_to_string(const char16_t *str, size_t len) {
            static auto convert = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{};
            return convert.to_bytes(str, str + len);
        }

        // interfaceToken is encoding in char16_t*. we should convert to char*
        static std::string u16string_to_string(const char16_t *str) {
            static auto convert = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{};
            return convert.to_bytes(str);
        }


        // android 12 and above
        status_t
        Parcel_writeInterfaceToken_proxy_12(void *Parcel, const char16_t *interface, size_t len) {
            BYTEHOOK_STACK_SCOPE();
            parcelToken.parcel = (uintptr_t) Parcel;
            parcelToken.token = u16string_to_string(interface, len);
            return BYTEHOOK_CALL_PREV(Parcel_writeInterfaceToken_proxy_12, Parcel, interface, len);
        }

        // before android 12
        status_t Parcel_writeInterfaceToken_proxy(void *Parcel, const void *interface) {
            BYTEHOOK_STACK_SCOPE();
            parcelToken.parcel = (uintptr_t) Parcel;
            parcelToken.token = u16string_to_string(((String16 *) interface)->mString);
            return BYTEHOOK_CALL_PREV(Parcel_writeInterfaceToken_proxy, Parcel, interface);
        }

        status_t IPCThreadState_transact_proxy(void *IPCThreadState,
                                               int32_t handle,
                                               uint32_t code,
                                               const void *data,
                                               void *reply,
                                               uint32_t flags) {
            BYTEHOOK_STACK_SCOPE();
            std::string name = "unknown";
            if (parcelToken.parcel == (uintptr_t) data) {
                name = parcelToken.token;
                // give up if writing on other thread.
                // don't block anything
                // rare case
                if (!tokens_write_flag.test_and_set(std::memory_order_acquire)) {
                    tokens.insert(name);
                    tokens_write_flag.clear(std::memory_order_release);
                }
            }
            ATRACE_RHEA_DRAFT("binder transact[%s:%d]", name.c_str(), code);
            return BYTEHOOK_CALL_PREV(IPCThreadState_transact_proxy,
                                      IPCThreadState, handle, code, data, reply, flags);
        }

        std::list<std::string> all_interface_token_names() {
            std::list<std::string> result;
            // spin until all thread writing complete
            while (tokens_write_flag.test_and_set(std::memory_order_acquire));
            for (const auto &token: tokens) {
                result.push_back(token);
            }
            tokens_write_flag.clear(std::memory_order_release);
            return result;
        }

    }  // namespace atrace
}  // namespace bytedance