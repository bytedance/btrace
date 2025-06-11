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
#include "Stack.h"

#include "../base/common_write.h"
#include "../utils/npth_dl.h"
#include <sstream>
#include <string>

namespace rheatrace {

static constexpr const char* ART_METHOD_PRETTY_METHOD = "_ZN3art9ArtMethod12PrettyMethodEb";

static constexpr const char* RUNTIME_METHOD_PREFIX = "<runtime method>";

bool Stack::sInited = false;
Stack::PrettyMethod Stack::sPrettyMethodCall = nullptr;

bool Stack::init(void* libartHandle) {
    if (sInited) {
        return true;
    }

    if (libartHandle != nullptr) {
        sPrettyMethodCall = reinterpret_cast<PrettyMethod>(npth_dlsym(libartHandle, ART_METHOD_PRETTY_METHOD));
        sInited = sPrettyMethodCall != nullptr;
    }

    return sInited;
}

std::string Stack::toString() {
    if (!sInited || sPrettyMethodCall == nullptr) {
        return "";
    }
    if (mActualDepth != mSavedDepth && mActualDepth < MAX_STACK_DEPTH) {
        return "";
    }
    std::ostringstream oss;
    for (int i = 0; i < mSavedDepth; ++i) {
        auto artMethod = mStackMethods[i];
        if (artMethod) {
            auto pretty = sPrettyMethodCall(reinterpret_cast<void*>(artMethod), true);
            if (strncmp(pretty.c_str(), RUNTIME_METHOD_PREFIX, 16) != 0) {
                oss << "  at " << pretty << "\n";
            } else {
                // 包含 <runtime method> 的栈帧需要从栈深度记录中排除掉
                mActualDepth -= 1;
            }
        }
    }
    return oss.str();
}

std::string Stack::toString(void* ptr) {
    if (!sInited || sPrettyMethodCall == nullptr || ptr == nullptr) {
        return "";
    }
    return sPrettyMethodCall(ptr, true);
}

uint32_t Stack::encodeInfo(char* out, std::unordered_set<uint64_t>* set) {
    int size = 0;
    size += rheatrace::writeBuf(out + size, mSavedDepth);
    size += rheatrace::writeBuf(out + size, mActualDepth);
    for (int i = 0; i < mSavedDepth; ++i) {
        size += rheatrace::writeBuf(out + size, mStackMethods[i]);
        if (set != nullptr) {
            set->insert(mStackMethods[i]);
        }
    }
    return size;
}

} // namespace rheatrace

