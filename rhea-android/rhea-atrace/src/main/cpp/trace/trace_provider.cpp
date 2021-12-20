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

#include "trace_provider.h"

#include <stdlib.h>

#include <sys/system_properties.h>

#define LOG_TAG "Rhea.provider"
#include <utils/debug.h>

namespace bytedance {
namespace atrace {

TraceProvider& TraceProvider::Get() {
  static TraceProvider kInstance;
  return kInstance;
}

TraceProvider::TraceProvider() = default;

TraceProvider::~TraceProvider() = default;

void TraceProvider::SetConfig(ConfigKey key, int64_t val) {
    atrace_config_[key] = val;
}

bool TraceProvider::IsEnableIO() const {
    return atrace_config_[kIO] == 1;
}

bool TraceProvider::isMainThreadOnly() const {
    return atrace_config_[KMainThreadOnly] == 2;
}

bool TraceProvider::isEnableMemory()  const {
    return atrace_config_[kMemory] == 4;
}

bool TraceProvider::isEnableClassLoad() const {
    return atrace_config_[kClassLoad] == 8;
}

void TraceProvider::SetMainThreadId(pid_t tid) {
    main_thread_id_ = tid;
}

pid_t TraceProvider::GetMainThreadId() const {
    return main_thread_id_;
}

void TraceProvider::SetBufferSize(size_t buffer_size) {
    buffer_size_ = buffer_size;
}

size_t TraceProvider::GetBufferSize() const {
    return buffer_size_;
}

void TraceProvider::SetBlockHookLibs(const std::string &blockHookLibs) {
    std::string self_lib = "rhea-trace";
    if (!blockHookLibs.empty()) {
        std::string pattern = ",";
        std::string fullStr = blockHookLibs + pattern;
        size_t position = fullStr.find(pattern);
        while (position != std::string::npos) {
            std::string temp = fullStr.substr(0, position);
            if (temp != self_lib) {
                block_hook_libs.push_back("lib" + temp + ".so");
            }
            fullStr = fullStr.substr(position + 1, fullStr.size());
            position = fullStr.find(pattern);
        }
    }
    block_hook_libs.emplace_back("lib" + self_lib + ".so");
}

const std::vector<std::string>& TraceProvider::GetBlockHookLibs() {
    return block_hook_libs;
}

void TraceProvider::SetTraceFolder(const std::string& trace_folder) {
    trace_folder_ = trace_folder;
}

const std::string& TraceProvider::GetTraceFolder() {
    return trace_folder_;
}

}  // namespace atrace
}  // namespace bytedance
