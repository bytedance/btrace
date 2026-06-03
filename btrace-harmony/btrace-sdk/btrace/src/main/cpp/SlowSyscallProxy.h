/*
 * Copyright (c) 2026 Bytedance Inc.
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

//
// Created on 2026/4/7.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef BTRACE_HARMONY_SLOWSYSCALLPROXY_H
#define BTRACE_HARMONY_SLOWSYSCALLPROXY_H

#include <mutex>
#include <functional>

#include "StackTrace.h"
#include "util/globals.h"

namespace btrace {

class SlowSysCallProxy {
public:
    using Action = void(*)(Type, void*, bool*);
    static bool Setup(bool disableSigMask, int sig, Action f);
private:
    static void InstallProxy();

    static inline std::once_flag slow_syscall_proxy_flag_;
    
    DISALLOW_COPY_AND_ASSIGN(SlowSysCallProxy);
};

}

#endif //BTRACE_HARMONY_SLOWSYSCALLPROXY_H
