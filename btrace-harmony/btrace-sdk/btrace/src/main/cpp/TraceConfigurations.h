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
// Created on 2025/10/9.
//

#ifndef HARMONY_BTRACE_TRACECONFIGURATION_H
#define HARMONY_BTRACE_TRACECONFIGURATION_H

#include <cstdint>
#include <napi/native_api.h>

namespace btrace {

class TraceConfigurations;

class BufferConfig {
public:
    BufferConfig() = default;
    uint32_t getBufferSize() const {
        return bufferSize;
    }
    bool getEnablePreload() const {
        return enablePreload;
    }
private:
    // in-memory buffer size for caching tracing stack traces
    // in slot unit not byte unit
    uint32_t bufferSize;
    bool enablePreload;
    
    friend class TraceConfigurations;
};

class UnwindConfig {
public:
    UnwindConfig() = default;
    uint32_t getMainThreadSampleInterval() const {
        return mainThreadSampleInterval;
    }
    uint32_t getSubThreadSampleInterval() const {
        return subThreadSampleInterval;
    }
    uint32_t getMainAsyncInterval() const {
        return mainAsyncInterval;
    }
    uint32_t getChildAsyncInterval() const {
        return childAsyncInterval;
    }
    bool isSignalBacktraceDisabled() const  {
        return disableSignalBacktrace;
    }
    bool isForceLoad() const {
        return forceLoad;
    }
    bool isHighFreq() const {
        return highFreq;
    }
    bool isCustomUnwindDisabled() const {
        return disableCustomUnwind;
    }
    bool isEnableOsThread() const {
        return enableOsThread;
    }
    bool isDisableSigMask() const {
        return disableSigMask;
    }
    uint32_t getBufferedTime() const {
        return bufferedTime;
    }
    bool isParseElfSymbolsEnabled() const {
        return parseElfSymbols;
    }
private:
    // sampling interval in milli-seconds for main thread
    uint32_t mainThreadSampleInterval;
    // sampling interval in milli-seconds for sub-threads
    uint32_t subThreadSampleInterval;
    uint32_t mainAsyncInterval = 0;
    uint32_t childAsyncInterval = 0;
    bool disableSignalBacktrace;
    bool forceLoad;
    bool highFreq = false;
    bool disableCustomUnwind;
    bool enableOsThread = false;
    bool disableSigMask = false;
    bool parseElfSymbols = false;
    uint32_t bufferedTime = 0;
public:
    friend class TraceConfigurations;
};

class HookConfig {
public:
    HookConfig() = default;
    int32_t getMagic() const {
        return magic;
    }
private:
    int32_t magic{0};
    friend class TraceConfigurations;
};

class TraceConfigurations {
public:
    static TraceConfigurations& get();
    
    void update(napi_env env, napi_value configArray, uint32_t length);
    
    bool isEnabled() {
        return enabled;
    }
    
    const BufferConfig& getBufferConfig() {
        return bufferConfig;
    }
    
    const UnwindConfig& getUnwindConfig() {
        return unwindConfig;
    }
    
    const HookConfig& getHookConfig() {
        return hookConfig;
    }
    
private:
    TraceConfigurations() = default;
    TraceConfigurations(const TraceConfigurations&) = delete;
    TraceConfigurations& operator=(const TraceConfigurations&) = delete;
    
    // global control flag, when flag is disabled, we can't start any tracing.
    bool enabled;
    BufferConfig bufferConfig;
    UnwindConfig unwindConfig;
    HookConfig hookConfig;
};

}

#endif //HARMONY_BTRACE_TRACECONFIGURATION_H
