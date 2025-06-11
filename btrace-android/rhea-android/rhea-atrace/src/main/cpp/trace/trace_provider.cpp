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

void TraceProvider::SetCategories(std::vector<std::string> categories) {
    this->categories_ = std::move(categories);
}

static bool IsCategoryEnable(const char* name) {
    char value[PROP_VALUE_MAX];
    __system_property_get(name, value);
    if(value[0] == '1') {
        return true;
    }
    char all[PROP_VALUE_MAX];
    __system_property_get("debug.rhea.category.all", all);
    return all[0] == '1';
}

bool TraceProvider::IsEnableIO() const {
    return IsCategoryEnable("debug.rhea.category.io");
}

bool TraceProvider::isEnableMemory() const {
    return IsCategoryEnable("debug.rhea.category.memory");
}

bool TraceProvider::isEnableClassLoad() const {
    return IsCategoryEnable("debug.rhea.category.class");
}

bool TraceProvider::isEnableThread() const {
    return IsCategoryEnable("debug.rhea.category.thread");
}

bool TraceProvider::isEnableBinder() const {
    return IsCategoryEnable("debug.rhea.category.binder");
}

bool TraceProvider::isEnableDetailRender() const {
    return IsCategoryEnable("debug.rhea.category.render");
}

bool TraceProvider::isMainThreadOnly() const {
    char value[PROP_VALUE_MAX];
    __system_property_get("debug.rhea.mainThreadOnly", value);
    return value[0] == '1';
}

void TraceProvider::SetMainThreadId(pid_t tid) {
    main_thread_id_ = tid;
}

void TraceProvider::SetMainThreadOnly(bool mainOnly) {
    mainThreadOnly_ = mainOnly;
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
