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

#include "TraceDumper.h"
#include "ThreadNames.h"
#include <bits/alltypes.h>
#include <cassert>
#include <string>
#include "TracePreloader.h"
#include "TraceUnwinder.h"
#include "util/globals.h"
#include "util/time_utils.h"
#include "util/smart_fd.h"
#include <cstdint>
#include <fcntl.h>
#include <hidebug/hidebug.h>
#include <hilog/log.h>
#include <sys/mman.h>
#include <unordered_set>
#include <sstream>
#include <sys/resource.h>
#include <thread>
#include <set>

#include "util/globals.h"

#undef LOG_TAG
#define LOG_TAG "btrace:Dumper"

namespace btrace {

TraceDumper& TraceDumper::get() {
    static TraceDumper inst{};
    return inst;
}

template<typename T>
uint32_t writeBuf(char* addr, T data) {
    memcpy(addr, &data, sizeof(T));
    return sizeof(T);
}

template<typename T>
uint32_t writeBufArray(char* addr, T* a, size_t len) {
    uint32_t size = sizeof(T) * len;
    memcpy(addr, a, size);
    return size;
}

constexpr size_t STACK_TRACE_DUMP_SIZE = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(StackTrace::timestamp)
    + sizeof(StackTrace::cpuTime) + sizeof(StackTrace::depth) + sizeof(StackTrace::elements);


class TraceMappingDumper {
public:
    TraceMappingDumper(std::string& mappingPath, uint32_t totalProducers, uint64_t begin, uint64_t end, TraceUnwinder& unwinder)
    : traceMappingPath(mappingPath), producerCount(totalProducers), reservedCapacity((end - begin) * 4), unwinder(unwinder) {
        slots_ = new ProducerSlot[totalProducers];
        for (uint32_t i = 0; i < totalProducers; ++i) {
            auto& slot = slots_[i];
            slot.pcs.reserve(reservedCapacity / 8);
            slot.tids.reserve(256);
        }
    }
    void addMethodPointers(size_t idx, int32_t tid, uintptr_t elements[], uint32_t len);
    void markProducerDone(size_t idx);
    void run();
    void join();
private:
    void innerRun();
    std::string& traceMappingPath;
    TraceUnwinder& unwinder;
    uint32_t producerCount;
    uint64_t reservedCapacity;
    struct ProducerSlot {
        std::mutex mutex{};
        std::unordered_set<uintptr_t> pcs{};
        std::unordered_set<int32_t> tids{};
        bool done{false};
    };
    ProducerSlot* slots_{nullptr};
    std::thread* t_{nullptr};
    
};

void TraceMappingDumper::addMethodPointers(size_t idx, int32_t tid, uintptr_t elements[], uint32_t len) {
    auto& slot = slots_[idx];
    std::lock_guard<std::mutex> lock(slot.mutex);
    slot.tids.emplace(tid);
    if (len > 0) {
        slot.pcs.insert(elements, elements+len);
    }
}

void TraceMappingDumper::markProducerDone(size_t idx) {
    auto& slot = slots_[idx];
    std::lock_guard<std::mutex> lock(slot.mutex);
    slot.done = true;
}

void TraceMappingDumper::run() {
    t_ = new std::thread([this]{
        innerRun();
    });
}

void TraceMappingDumper::innerRun() {
    
    auto unwindMonoStart = current_monotime_millis();
    auto unwindCpuStart = thread_cpu_time_millis();
    
    setpriority(PRIO_PROCESS, gettid(), -19);
    
    int mappingFd = open(traceMappingPath.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
    if (mappingFd == -1) {
        OH_LOG_ERROR(LOG_APP, "open trace mapping file failed: %{public}s", strerror(errno));
        return;
    }
    SmartFd smartMappingFd(mappingFd);
    uint64_t magic = 0;
    uint32_t version = 1;
    write(mappingFd, &magic, sizeof(magic));
    write(mappingFd, &version, sizeof(version));
    std::set<uintptr_t> realPcs{};
    std::set<std::string> buildIdMap{};
    std::set<int32_t> tids{};
    std::ostringstream buildIdOss{};
    std::ostringstream methodMappingOss{};
    auto obj = OH_HiDebug_CreateBacktraceObject();
    if (!obj) {
        OH_LOG_ERROR(LOG_APP, "create backtrace object failed");
        return;
    }

    while (true) {
        std::set<uintptr_t> tmp{};
        uint32_t producerDoneCount = 0;
        uint32_t curRoundRecordCount = 0;
        for (uint32_t i = 0; i < producerCount; ++i) {
            auto& slot = slots_[i];
            std::lock_guard<std::mutex> lock(slot.mutex);
            if (slot.pcs.size() > 0) {
                for (auto it = slot.pcs.begin(); it != slot.pcs.end(); ++it) {
                    if (realPcs.find(*it) == realPcs.end()) {
                        tmp.emplace(*it);
                        realPcs.emplace(*it);
                    }
                }
                curRoundRecordCount += slot.pcs.size();
                slot.pcs.clear();
            }
            if (slot.tids.size() > 0) {
                for (auto it = slot.tids.begin(); it != slot.tids.end(); ++it) {
                    tids.emplace(*it);
                }
                slot.tids.clear();
            }
            if (slot.done) {
                producerDoneCount += 1;
            }
        }
        if (tmp.size() > 0) {
            methodMappingOss.clear();
//            OH_LOG_INFO(LOG_APP, "start batch unwind %{public}zu records", tmp.size());
            unwinder.batchUnwind(obj, tmp, methodMappingOss, buildIdOss, buildIdMap);
//            OH_LOG_INFO(LOG_APP, "finish batch unwind");
            std::string contents = methodMappingOss.str();
            uint32_t contentLen = contents.size();
            write(mappingFd, &contentLen, sizeof(contentLen));
            write(mappingFd, contents.c_str(), contentLen);
        }
        if (producerDoneCount >= producerCount) {
            break;
        }
        if (curRoundRecordCount == 0) {
            sched_yield();
        }
    }
    OH_HiDebug_DestroyBacktraceObject(obj);
    // mark method mapping end
    uint32_t methodMappingEndFlag = 0x01010101;
    write(mappingFd, &methodMappingEndFlag, sizeof(methodMappingEndFlag));
    // write build ids
    std::string buildIdContents = buildIdOss.str();
    uint32_t buildIdLength = buildIdContents.size();
    write(mappingFd, &buildIdLength, sizeof(buildIdLength));
    write(mappingFd, buildIdContents.c_str(), buildIdLength);
    // write thread names
    if (TraceUnwinder::get().isOsThreadEnabled()) {
        std::string threadNameContents = ThreadNames::get().batchUnwindThreadNames(tids);
        uint32_t threadNamesLength = threadNameContents.size();
        write(mappingFd, &threadNamesLength, sizeof(threadNamesLength));
        write(mappingFd, threadNameContents.c_str(), threadNamesLength);
    }
    
    delete [] slots_;
    auto unwindMonoEnd = current_monotime_millis();
    auto unwindCpuEnd = thread_cpu_time_millis();
    OH_LOG_INFO(LOG_APP, "unwind cost: %{public}zums / %{public}zums", (unwindMonoEnd - unwindMonoStart), (unwindCpuEnd - unwindCpuStart));
}

void TraceMappingDumper::join() {
    if (t_) {
        t_->join();
    }
}

class ThreadedTraceBufferDumper {
public:
    ThreadedTraceBufferDumper(size_t id, bool writeMeta, std::string& traceDir, std::string& extra,
                              TraceBuffer& buffer, uint64_t begin, uint64_t end, TraceMappingDumper& mappingDumper,
                              const std::vector<pid_t> &tids, uint64_t beginTimeNs=0, uint64_t endTimeNs=0);
    
    void run();
    
    uint32_t join();
private:
    void innerRun();
    size_t id;
    bool writeMeta;
    std::string traceBinPath;
    std::string& extra;
    TraceBuffer& buffer;
    uint64_t beginTicket;
    uint64_t endTicket;
    TraceMappingDumper& mappingDumper;
    std::thread* t;
    uint32_t dumpCount;
    
    uint64_t beginTimeNs_;
    uint64_t endTimeNs_;
    std::unordered_set<pid_t> tids_;
};

ThreadedTraceBufferDumper::ThreadedTraceBufferDumper(size_t id, bool writeMeta, std::string& traceDir, std::string& extra, TraceBuffer& buffer,
                                                     uint64_t begin, uint64_t end, TraceMappingDumper& mappingDumper,
                                                     const std::vector<pid_t> &tids, uint64_t beginTimeNs, uint64_t endTimeNs)
: id(id), writeMeta(writeMeta), traceBinPath(traceDir), extra(extra), buffer(buffer), beginTicket(begin),
endTicket(end), mappingDumper(mappingDumper), t(nullptr), dumpCount(0), tids_(tids.begin(), tids.end()),
beginTimeNs_(beginTimeNs), endTimeNs_(endTimeNs) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "/trace-%zu.bin", id);
    traceBinPath += buf;
}

void ThreadedTraceBufferDumper::run() {
    t = new std::thread([this]{
        innerRun();
    });
}

void ThreadedTraceBufferDumper::innerRun() {
    auto readMonoStart = current_monotime_millis();
    auto readCpuStart = thread_cpu_time_millis();
    setpriority(PRIO_PROCESS, gettid(), -10);
    int fd = open(traceBinPath.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd == -1) {
        OH_LOG_ERROR(LOG_APP, "open target dump file failed: %{public}s", strerror(errno));
        return;
    }
    SmartFd smartTraceFd(fd);
    auto end = endTicket;
    auto begin = beginTicket;
    if (end <= begin) {
        return;
    }
    uint64_t magic = 0;
    uint32_t type = 1;
    uint32_t os = 4;
    uint32_t version = 1;
    uint32_t extraLen = extra.length();
    auto mapSize = STACK_TRACE_DUMP_SIZE * (end - begin);
    if (writeMeta) {
        mapSize += (sizeof(magic) + sizeof(type) + sizeof(os) + sizeof(version) + sizeof(extraLen) + extraLen);
    }
    uint64_t pageMask = getpagesize() - 1;
    mapSize = ((uint64_t) mapSize + pageMask) & ~pageMask; 
    if (ftruncate(fd, mapSize) != 0) {
        OH_LOG_ERROR(LOG_APP, "truncate target dump file failed: %{public}s", strerror(errno));
        return;
    }
    void* addr = mmap(nullptr, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        OH_LOG_ERROR(LOG_APP, "mmap(%{public}lu) for dump failed: %{public}s", mapSize, strerror(errno));
        ftruncate(fd, 0);
        return;
    }
    uint32_t offset = 0;
    char* buf = (char*) addr;
    if (writeMeta) {
        offset += writeBuf(buf + offset, magic);
        offset += writeBuf(buf + offset, type);
        offset += writeBuf(buf + offset, os);
        offset += writeBuf(buf + offset, version);
        offset += writeBuf(buf + offset, extraLen);
        offset += writeBufArray(buf + offset, extra.c_str(), extra.length());
    } else {
        magic = 1;
        offset += writeBuf(buf + offset, magic);
    }
    size_t count = 0;
    StackTrace st{};
    for (int i = end - 1; i >= begin; i--) {
        if (!buffer.read(i, &st)) {
            break;
        }
        
        if (0 < beginTimeNs_ && st.timestamp < beginTimeNs_) {
            continue;
        }
    
        if (0 < endTimeNs_ && endTimeNs_ < st.timestamp) {
            continue;
        }
        
        if (0 < tids_.size() && tids_.find(st.tid) == tids_.end()) {
            continue;
        }
        
        offset += writeBuf(buf + offset, (uint32_t) st.type);
        offset += writeBuf(buf + offset, (uint32_t) st.tid);
        offset += writeBuf(buf + offset, st.timestamp);
        offset += writeBuf(buf + offset, st.cpuTime);
        offset += writeBuf(buf + offset, st.depth);
        if (st.depth > 0) {
            offset += writeBufArray(buf + offset, st.elements, st.depth);
        }
        mappingDumper.addMethodPointers(id, st.tid, st.elements, st.depth);
        count++;
    }
    mappingDumper.markProducerDone(id);
    msync(addr, offset, MS_SYNC);
    munmap(addr, mapSize);
    ftruncate(fd, offset);
    dumpCount = count;
    auto readMonoEnd = current_monotime_millis();
    auto readCpuEnd = thread_cpu_time_millis();
    OH_LOG_INFO(LOG_APP, "read buffer %{public}zu cost: %{public}zums / %{public}zums", id, (readMonoEnd - readMonoStart), (readCpuEnd - readCpuStart));
}

uint32_t ThreadedTraceBufferDumper::join() {
    if (t) {
        t->join();
        delete t;
        return dumpCount;
    }
    return 0;
}

class TracePreloadMappingDumper {
public:
    TracePreloadMappingDumper(std::string& mappingPath, TraceUnwinder& unwinder)
    : traceMappingPath_(mappingPath), unwinder_(unwinder) {}
    void addMethodPointers(pid_t tid, uintptr_t elements[], uint32_t len);
    void dump();
private:
    std::set<uintptr_t> pcs_{};
    std::set<int32_t> tids_{};
    std::string& traceMappingPath_;
    TraceUnwinder& unwinder_;
};

void TracePreloadMappingDumper::addMethodPointers(pid_t tid, uintptr_t elements[], uint32_t len) {
    tids_.emplace(tid);
    
    if (0 < len) {
        pcs_.insert(elements, elements+len);
    }
}

void TracePreloadMappingDumper::dump() {
    auto unwindMonoStart = current_monotime_millis();
    auto unwindCpuStart = thread_cpu_time_millis();
    
    int mappingFd = open(traceMappingPath_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
    if (mappingFd == -1) {
        OH_LOG_ERROR(LOG_APP, "open trace mapping file failed: %{public}s", strerror(errno));
        return;
    }
    SmartFd smartMappingFd(mappingFd);
    uint64_t magic = 0;
    uint32_t version = 1;
    write(mappingFd, &magic, sizeof(magic));
    write(mappingFd, &version, sizeof(version));
    
    std::set<std::string> buildIdMap{};
    std::ostringstream buildIdOss{};
    std::ostringstream methodMappingOss{};
    auto obj = OH_HiDebug_CreateBacktraceObject();
    
    if (!obj) {
        OH_LOG_ERROR(LOG_APP, "create backtrace object failed");
        return;
    }
    
    if (0 < pcs_.size()) {
        unwinder_.batchUnwind(obj, pcs_, methodMappingOss, buildIdOss, buildIdMap);
        std::string contents = methodMappingOss.str();
        uint32_t contentLen = contents.size();
        write(mappingFd, &contentLen, sizeof(contentLen));
        write(mappingFd, contents.c_str(), contentLen);
    }
    
    OH_HiDebug_DestroyBacktraceObject(obj);
    // mark method mapping end
    uint32_t methodMappingEndFlag = 0x01010101;
    write(mappingFd, &methodMappingEndFlag, sizeof(methodMappingEndFlag));
    // write build ids
    std::string buildIdContents = buildIdOss.str();
    uint32_t buildIdLength = buildIdContents.size();
    write(mappingFd, &buildIdLength, sizeof(buildIdLength));
    write(mappingFd, buildIdContents.c_str(), buildIdLength);
    // write thread names
    if (TraceUnwinder::get().isOsThreadEnabled()) {
        std::string threadNameContents = ThreadNames::get().batchUnwindThreadNames(tids_);
        uint32_t threadNamesLength = threadNameContents.size();
        write(mappingFd, &threadNamesLength, sizeof(threadNamesLength));
        write(mappingFd, threadNameContents.c_str(), threadNamesLength);
    }
    
    auto unwindMonoEnd = current_monotime_millis();
    auto unwindCpuEnd = thread_cpu_time_millis();
    OH_LOG_INFO(LOG_APP, "TracePreloadMappingDumper::dump() cost: %{public}zums / %{public}zums", (unwindMonoEnd - unwindMonoStart), (unwindCpuEnd - unwindCpuStart));
}

class TracePreloadBufferDumper {
public:
    TracePreloadBufferDumper(std::string& traceDir, std::string& extra, TraceBuffer& buffer,
                             TracePreloadMappingDumper& mappingDumper, const std::vector<pid_t> &tids,
                             uint64_t beginTimeNs=0, uint64_t endTimeNs=0)
                             : traceBinPath_(traceDir), extra_(extra), buffer_(buffer),
                             mappingDumper_(mappingDumper), tids_(tids.begin(), tids.end()),
                             beginTimeNs_(beginTimeNs), endTimeNs_(endTimeNs) {
        traceBinPath_ += "/trace-0.bin";
    }
    
    void dump();
    uint32_t dumpCount() { return dumpCount_; }
private:
    std::string traceBinPath_;
    std::string& extra_;
    TraceBuffer& buffer_;
    TracePreloadMappingDumper& mappingDumper_;
    
    uint64_t beginTimeNs_;
    uint64_t endTimeNs_;
    std::unordered_set<pid_t> tids_;
    uint32_t dumpCount_ = 0;
};

void TracePreloadBufferDumper::dump() {
    auto readMonoStart = current_monotime_millis();
    auto readCpuStart = thread_cpu_time_millis();
    
    auto tempBuffer = std::vector<SampleRecord>();
    auto stackIdMap = std::unordered_map<uint64_t, std::vector<uint64_t>>();

    TracePreloader::Lock();
    buffer_.preloadIterate([this, &tempBuffer, &stackIdMap](const SampleRecord &record) {
        if (0 < beginTimeNs_ && record.data.timestamp < beginTimeNs_) {
            return;
        }
    
        if (0 < endTimeNs_ && endTimeNs_ < record.data.timestamp) {
            return;
        }
        
        if (0 < tids_.size() && tids_.find(record.data.tid) == tids_.end()) {
            return;
        }
        
        auto stackId = record.stack_id;
        auto it = stackIdMap.find(stackId);
        if (it == stackIdMap.end()) {
            auto stack = TracePreloader::GetStackById(stackId);
            
            if (0 < stack.size()) {
                bool inserted;
                std::tie(it, inserted) = stackIdMap.emplace(stackId, std::move(stack));
            }
        }
    
        tempBuffer.emplace_back(record);
    });
    TracePreloader::Unlock();
    
    if (tempBuffer.size() == 0) { return; }

    int fd = open(traceBinPath_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd == -1) {
        OH_LOG_ERROR(LOG_APP, "open target dump file failed: %{public}s", strerror(errno));
        return;
    }
    
    SmartFd smartTraceFd(fd);
    
    uint64_t magic = 0;
    uint32_t type = 1;
    uint32_t os = 4;
    uint32_t version = 1;
    uint32_t extraLen = extra_.length();
    
    auto mapSize = STACK_TRACE_DUMP_SIZE * tempBuffer.size();
    mapSize += (sizeof(magic) + sizeof(type) + sizeof(os) + sizeof(version) + sizeof(extraLen) + extraLen);
    
    uint64_t pageMask = getpagesize() - 1;
    mapSize = ((uint64_t) mapSize + pageMask) & ~pageMask; 
    if (ftruncate(fd, mapSize) != 0) {
        OH_LOG_ERROR(LOG_APP, "truncate target dump file failed: %{public}s", strerror(errno));
        return;
    }
    
    void* addr = mmap(nullptr, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        OH_LOG_ERROR(LOG_APP, "mmap(%{public}lu) for dump failed: %{public}s", mapSize, strerror(errno));
        ftruncate(fd, 0);
        return;
    }
    
    uint32_t offset = 0;
    char* buf = (char*) addr;
    offset += writeBuf(buf + offset, magic);
    offset += writeBuf(buf + offset, type);
    offset += writeBuf(buf + offset, os);
    offset += writeBuf(buf + offset, version);
    offset += writeBuf(buf + offset, extraLen);
    offset += writeBufArray(buf + offset, extra_.c_str(), extra_.length());
    
    size_t count = 0;
    for (int i = 0; i < tempBuffer.size(); i++) {
        auto &st = tempBuffer[i];
        auto it = stackIdMap.find(st.stack_id);
        if (it == stackIdMap.end()) {
            continue;
        }
        
        uint32_t depth = it->second.size();
        offset += writeBuf(buf + offset, (uint32_t) st.data.type);
        offset += writeBuf(buf + offset, (uint32_t) st.data.tid);
        offset += writeBuf(buf + offset, st.data.timestamp);
        offset += writeBuf(buf + offset, st.data.cpuTime);
        offset += writeBuf(buf + offset, depth);
    
        if (0 < depth) {
            offset += writeBufArray(buf + offset, it->second.data(), depth);
        }
        
        mappingDumper_.addMethodPointers(st.data.tid, it->second.data(), depth);
        count ++;
    }

    msync(addr, offset, MS_SYNC);
    munmap(addr, mapSize);
    ftruncate(fd, offset);
    dumpCount_ = count;

    auto readMonoEnd = current_monotime_millis();
    auto readCpuEnd = thread_cpu_time_millis();
    OH_LOG_INFO(LOG_APP, "TracePreloadBufferDumper::dump() cost: %{public}zums / %{public}zums", (readMonoEnd - readMonoStart), (readCpuEnd - readCpuStart));
}

uint32_t TraceDumper::dump(TraceBuffer& buffer, TraceUnwinder& unwinder, std::string& traceDir, std::string& extra,
                            const std::vector<pid_t> &tids, uint64_t beginTimeMs, uint64_t endTimeMs) {
    auto monoStart = current_monotime_millis();
    auto cpuStart = thread_cpu_time_millis();
    constexpr uint32_t traceDumperThreadCount = 3;
    uint32_t count = 0;
    std::string traceMappingPath = traceDir + "/mapping.bin";
    
    int64_t beginTimeNs = beginTimeMs * kNanosPerMilli;
    int64_t endTimeNs = endTimeMs * kNanosPerMilli;
    
    if (buffer.isPreloadEnabled()) {
        TracePreloader::ProcessBuffer();
        TracePreloadMappingDumper mappingDumper(traceMappingPath, unwinder);
        TracePreloadBufferDumper bufferDumper(traceDir, extra, buffer, mappingDumper, tids, beginTimeNs, endTimeNs);
        bufferDumper.dump();
        count = bufferDumper.dumpCount();
        
        if (0 < count) {
            mappingDumper.dump();
        }
    } else {
        uint64_t beginTicket = 0, endTicket = 0;
        buffer.getCurTicketRange(&beginTicket, &endTicket);
        if (endTicket <= beginTicket) {
            OH_LOG_ERROR(LOG_APP, "empty trace buffer");
            return 0;
        }
        TraceMappingDumper mappingDumper(traceMappingPath, traceDumperThreadCount, beginTicket, endTicket, unwinder);
        auto step = (endTicket - beginTicket) / traceDumperThreadCount;
        ThreadedTraceBufferDumper bufferDumper0(0, true, traceDir, extra, buffer,
                                                beginTicket, beginTicket + step,
                                                mappingDumper, tids, beginTimeNs, endTimeNs);
        ThreadedTraceBufferDumper bufferDumper1(1, false, traceDir, extra, buffer,
                                                beginTicket + step, beginTicket + step * 2,
                                                mappingDumper, tids, beginTimeNs, endTimeNs);
        ThreadedTraceBufferDumper bufferDumper2(2, false, traceDir, extra, buffer,
                                                beginTicket + step * 2, endTicket,
                                                mappingDumper, tids, beginTimeNs, endTimeNs);
        mappingDumper.run();
        bufferDumper0.run();
        bufferDumper1.run();
        bufferDumper2.run();
        count += bufferDumper0.join();
        count += bufferDumper1.join();
        count += bufferDumper2.join();
        mappingDumper.join();
    }

    auto monoEnd = current_monotime_millis();
    auto cpuEnd = thread_cpu_time_millis();
    OH_LOG_INFO(LOG_APP, "dump succeed, cost: %{public}zums / %{public}zums, count: %{public}u", (monoEnd - monoStart), (cpuEnd - cpuStart), count);
    return count;
}

}