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

#include <jni.h>
#include "../base/PerfCollectorBaseImpl.h"
#include "SamplingRecord.h"
#include "SamplingConfig.h"
#include "StackVisitor.h"
#include "../utils/time.h"
#include <unistd.h>

namespace rheatrace {

/**
 * Collector of sampling stack trace. Major usage of this class is calling request() method in our
 * preset trace point to capture java stack synchronously and saved it to buffer inside this
 * collector.
 */
class SamplingCollector : public PerfCollectorBaseImpl<rheatrace::TYPE_SAMPLING, 5, false, SamplingRecord> {
public:
    static SamplingCollector* create(JNIEnv* env, jlongArray configs);

    static void destroy() {
        if (sInstance != nullptr) {
            delete sInstance;
            sInstance = nullptr;
        }
    }

    static SamplingCollector* getInstance() {
        return sInstance;
    }

    static bool
    request(SamplingType type, void* self = nullptr, bool force = false, bool captureAtEnd = false,
            uint64_t beginNano = 0, uint64_t beginCpuNano = 0);

    static void newJavaMessageWillBegin();

    void start(JNIEnv* env, jlongArray asyncConfigs) override;

    void updateConfigs(JNIEnv* env, jlongArray configs) override;

    void stop() override {
        paused = true;
    }

    int64_t write(SamplingRecord& r) {
        return mBuffer->write(r);
    }

    bool isPaused() const {
        return paused;
    }

protected:

    Dumper* newDumper() override;

    const char* getDumpPerfFileName() override;

    const char* getDumpMappingFileName() override;

private:

    SamplingCollector(PerfBuffer<SamplingRecord>* buffer, SamplingConfig& config)
            : PerfCollectorBaseImpl<rheatrace::TYPE_SAMPLING, 5, false, SamplingRecord>(buffer),
              config(config), paused(false) {
    }

    static SamplingCollector* sInstance;
    SamplingConfig config;
    bool paused;
};

class ScopeSampling {
public:
    uint64_t beginNano_;
private:
    void* self_;
    SamplingType type_;
    uint64_t beginCpuNano_;
    bool force_;
    bool condition_;
    pid_t tid;
public:
    explicit ScopeSampling(SamplingType type, void *self = nullptr, bool force = false) : type_(type), self_(self), force_(force), condition_(true), tid(gettid()) {
        beginNano_ = current_boot_time_nanos();
        beginCpuNano_ = current_thread_cpu_time_nanos();
    }

    ScopeSampling(ScopeSampling&) = delete;

    void setCondition(bool cond) {
        condition_ = cond;
    }

    ~ScopeSampling() {
        if (condition_) {
            SamplingCollector::request(type_, self_, force_, true, beginNano_, beginCpuNano_);
        }
    }
};

} // namespace rheatrace