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
// Created on 2025/10/13.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONY_BTRACE_TRACEHOOKS_H
#define HARMONY_BTRACE_TRACEHOOKS_H

#include "TraceConfigurations.h"
#include <functional>
#include "StackTrace.h"

namespace btrace {

class TraceHooks {
public:
    using Action = void(*)(Type, void*);
    static TraceHooks& get();
    bool setup(const HookConfig& config, Action f);
    bool cleanup();
private:
    TraceHooks() = default;
    TraceHooks(const TraceHooks&) = delete;
    TraceHooks& operator=(const TraceHooks&) = delete;
};

}

#endif //HARMONY_BTRACE_TRACEHOOKS_H
