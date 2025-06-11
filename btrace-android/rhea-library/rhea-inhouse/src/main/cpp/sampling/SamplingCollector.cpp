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
#include "SamplingCollector.h"
#include "Stack.h"
#include "StackVisitor.h"
#include "../trace/SamplingTrace.h"
#include "../stat/JavaObjectStat.h"
#include "SamplingRecord.h"
#include <unistd.h>
#include <unordered_set>
#include <setjmp.h>
#include <sys/resource.h>
#include <dirent.h>
#include <string>

#include "../utils/time.h"
#include "../utils/misc.h"

#define LOG_TAG "RheaTrace:Sampling"
#include "../utils/log.h"


namespace rheatrace {

SamplingCollector* SamplingCollector::sInstance = nullptr;

static uint64_t getStackRecordTime(SamplingRecord& r) {
    return r.mEndNanoTime == 0 ? r.mNanoTime : r.mEndNanoTime;
}

SamplingCollector* SamplingCollector::create(JNIEnv* env, jlongArray rawConfig) {
    if (sInstance == nullptr) {
        SamplingConfig config(env, rawConfig);
        auto* buffer = PerfBuffer<SamplingRecord>::create(config.capacity, getStackRecordTime);
        struct timespec ts{};
        clock_getres(config.clockId, &ts);
        ALOGI("clockId is %d, resolution is %ldns, visitKind is %d, interval is %ldns", config.clockId, ts.tv_nsec, config.stackWalkKind, config.mainThreadJavaIntervalNs);
        sInstance = new SamplingCollector(buffer, config);
    }
    return sInstance;
}

thread_local uint64_t lastJavaNano = 0;

thread_local uint32_t messageIndex = 0;

void SamplingCollector::newJavaMessageWillBegin() {
    messageIndex++;
}

bool SamplingCollector::request(SamplingType type, void* self, bool force, bool captureAtEnd,
                                uint64_t beginNano, uint64_t beginCpuNano) {
    auto* collector = SamplingCollector::getInstance();
    if (collector == nullptr || collector->isPaused()) {
        return false;
    }
    auto currentNano = current_clock_id_time_nanos(collector->config.clockId);
    if (force || currentNano - lastJavaNano > (is_main_thread() ? collector->config.mainThreadJavaIntervalNs
                                                                : collector->config.otherThreadJavaIntervalNs)) {
        lastJavaNano = currentNano;
        SamplingRecord r;
        if (StackVisitor::visitOnce(r.mStack, self, collector->config.stackWalkKind)) {
            if (r.mStack.mSavedDepth == 0 || r.mStack.mSavedDepth != r.mStack.mActualDepth) {
                return false;
            }
        } else {
            return false;
        }
        r.mType = type;
        r.mTid = gettid();
        r.mMessageId = messageIndex;

        auto& objectStat = JavaObjectStat::getAllocatedObjectStat();
        r.mAllocatedObjects = objectStat.objects;
        r.mAllocatedBytes = objectStat.bytes;
        if (collector->config.enableRusage) {
            struct rusage ru;
            if (getrusage(RUSAGE_THREAD, &ru) == 0) {
                r.mMajFlt = ru.ru_majflt;
                r.mNvCsw = ru.ru_nvcsw;
                r.mNivCsw = ru.ru_nivcsw;
            }
        }
        if (captureAtEnd) {
            r.mNanoTime = beginNano;
            r.mCpuTime = beginCpuNano;
            r.mEndNanoTime = currentNano;
            r.mEndCpuTime = current_thread_cpu_time_nanos();
        } else {
            r.mNanoTime = current_boot_time_nanos();
            r.mCpuTime = current_thread_cpu_time_nanos();
            r.mEndNanoTime = 0;
            r.mEndCpuTime = 0;
        }
        collector->write(r);
        return true;
    }
    return false;
}

void SamplingCollector::start(JNIEnv* env, jlongArray asyncConfigs) {
    paused = false;
    StackVisitor::init();
    trace::init(env, asyncConfigs, config.enableObjectAllocationStub, config.enableWakeup,
                config.shadowPauseMode);
}

class SamplingDumper : public Dumper {
private:
    std::unordered_set<uint64_t> mMethodIds;
    bool enableThreadNames;
public:
    explicit SamplingDumper(bool threadNames) : enableThreadNames(threadNames) {}

    uint32_t dumpRecord(JNIEnv* env, void* addr, void* r) override;

    bool hasMapping() override;

    bool dumpMapping(int fd) override;
};

Dumper* SamplingCollector::newDumper() {
    return new SamplingDumper(config.enabledThreadNames);
}

const char* SamplingCollector::getDumpPerfFileName() {
    return "sampling";
}

const char* SamplingCollector::getDumpMappingFileName() {
    return "sampling-mapping";
}

void SamplingCollector::updateConfigs(JNIEnv* env, jlongArray rawUpdatableConfig) {
    config.update(env, rawUpdatableConfig);
}

uint32_t SamplingDumper::dumpRecord(JNIEnv* env, void* addr, void* r) {
    SamplingRecord* record = reinterpret_cast<SamplingRecord*>(r);
    return record->encodeInto(reinterpret_cast<char*>(addr), &mMethodIds);
}

bool SamplingDumper::hasMapping() {
    return true;
}

thread_local struct sigaction preSEGVAction;
thread_local jmp_buf dumpMappingJmp;

void dumpMappingSIGSEGVHandler(int signo, siginfo_t *info, void *context) {
    if (sigaction(signo, &preSEGVAction, nullptr) != 0) {
        ALOGE("unregister signal %d handler failed: %m", signo);
    }
    siglongjmp(dumpMappingJmp, 1);
}

bool SamplingDumper::dumpMapping(int fd) {
    struct sigaction act{};
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = dumpMappingSIGSEGVHandler;
    if (sigaction(SIGSEGV, &act, &preSEGVAction) != 0) {
        ALOGE("sigaction failed.");
        return false;
    }
    if (sigsetjmp(dumpMappingJmp, 1) == 0) {
        uint64_t magic = 0;
        uint32_t version = 1;
        write(fd, &magic, sizeof(magic));
        write(fd, &version, sizeof(version));
        uint32_t count = mMethodIds.size();
        write(fd, &count, sizeof(count));
        for (const auto &item: mMethodIds) {
            write(fd, &item, sizeof(item));
            std::string symbol = Stack::toString(reinterpret_cast<void *>(item));
            uint16_t len = symbol.length();
            write(fd, &len, sizeof(len));
            auto buf = symbol.c_str();
            write(fd, buf, symbol.length());
        }
        // thread names
        if (enableThreadNames) {
            auto now = current_boot_time_millis();
            const char *task_dir = "/proc/self/task";
            DIR *dir = opendir(task_dir);
            if (dir != nullptr) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != nullptr) {
                    // Skip the current (.) and parent (..) entries
                    if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                        auto tid = (pid_t) atoi(entry->d_name);
                        char path[256];
                        snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
                        FILE *file = fopen(path, "r");
                        if (file) {
                            char thread_name[17];
                            if (fgets(thread_name, sizeof(thread_name), file) != nullptr) {
                                write(fd, &tid, 2);
                                thread_name[16] = 0;
                                uint8_t len = strlen(thread_name);
                                write(fd, &len, 1);
                                write(fd, thread_name, len);
                            }
                            fclose(file);
                        }
                    }
                }
            }
            closedir(dir);
            auto cost = current_boot_time_millis() - now;
            ALOGD("dump thread names cost %lums", cost);
        }
        return true;
    } else {
        return false;
    }
}

} // namespace rheatrace