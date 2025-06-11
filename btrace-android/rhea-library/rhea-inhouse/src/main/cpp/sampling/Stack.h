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


#include <unordered_set>


namespace rheatrace {

static constexpr uint32_t MAX_STACK_DEPTH = 128; // 抓栈深度小于等于 128 的数据有 97.92%（781997/798573），后续可以拆分多个 buffer 进一步优化

struct Stack {
private:
    using PrettyMethod = std::string (*)(void*, bool);
    static bool sInited;
    static PrettyMethod sPrettyMethodCall;
public:
    uint32_t mSavedDepth;
    uint32_t mActualDepth;
    uint64_t mStackMethods[MAX_STACK_DEPTH];

    uint32_t size() {
        return 4 + 4 + mSavedDepth * 8;
    }

    static uint32_t maxSize() {
        return 4 + 4 + MAX_STACK_DEPTH * 8;
    }

    uint32_t encodeInfo(char* out, std::unordered_set<uint64_t>* set);

    static bool init(void* libartHandle);
    std::string toString();
    static std::string toString(void* ptr);
};

} // namespace rheatrace