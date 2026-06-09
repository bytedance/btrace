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
// Created on 2025/10/15.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONY_BTRACE_TRACEDUMPER_H
#define HARMONY_BTRACE_TRACEDUMPER_H

#include "TraceBuffer.h"
#include <bits/alltypes.h>

#include <cstdint>
#include <vector>

#include "TraceUnwinder.h"

namespace btrace {

class TraceDumper {
public:
    static TraceDumper& get();
    uint32_t dump(TraceBuffer& buffer, TraceUnwinder& unwinder, std::string& traceDir,
                  std::string& extra, const std::vector<pid_t> &tids, uint64_t beginTime=0, uint64_t endTime=0);
private:
};

}


#endif //HARMONY_BTRACE_TRACEDUMPER_H
