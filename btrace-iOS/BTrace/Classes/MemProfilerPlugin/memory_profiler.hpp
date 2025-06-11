/*
 * Copyright (C) 2025 ByteDance Inc.
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

#ifndef BTRACE_MEMORY_PROFILER_H_
#define BTRACE_MEMORY_PROFILER_H_

#include <mach/vm_types.h>

#ifdef __cplusplus

#include <map>
#include <memory>
#include <atomic>

#include <malloc/malloc.h>

#include "utils.hpp"
#include "phmap.hpp"
#include "monitor.hpp"
#include "globals.hpp"
#include "ring_buffer.hpp"
#include "callstack_table.hpp"

#include "MemSampleModel.hpp"

namespace btrace
{

    void mem_profile_callback(std::vector<MemRecord> &processed_buffer);

    enum class MemEventType : uint32_t
    {
        Free = 0,
        Malloc = 1
    };

#pragma pack(push, 4)
    struct MemoryData 
    {
        size_t size;
        uint64_t address;
        uint64_t timestamp;
        uint32_t tid;
        uint32_t cpu_time;
        uint32_t alloc_size;
        uint32_t alloc_count;
        
        MemEventType event_type;
    };
#pragma pack(pop)

    struct MemorySample
    {
#if DEBUG || INHOUSE_TARGET || TEST_MODE
    static constexpr uintptr_t kAvgDepth = 32;
    static constexpr uintptr_t kMaxThreads = 128;
#else
    static constexpr uintptr_t kAvgDepth = 16;
    static constexpr uintptr_t kMaxThreads = 128;
#endif

        MemoryData data;
        uint32_t stack_size = 0;
        uword *pcs = nullptr;
    };

    class MemorySampleBuffer : public ConcurrentRingBuffer
    {
    public:
#if DEBUG
        MemorySampleBuffer(uintptr_t size) : ConcurrentRingBuffer(size, 64) {}
#else
        MemorySampleBuffer(uintptr_t size) : ConcurrentRingBuffer(size, 32) {}
#endif

        bool GetSample(RingBuffer *ring_buffer, MemorySample *sample);

        bool PutSample(MemorySample &sample);

    private:
        bool GetSampleFromBuffer(RingBuffer *buffer, RingBuffer::Buffer *buf, MemorySample *out);
    };

    class MemoryProfiler : public AllStatic
    {
    public:
        static void EnableProfiler(bool lite_mode=false, uint32_t interval=1,
                                   intptr_t period=1, intptr_t bg_period=1);
        static void DisableProfiler();

        static void Init();
        static void Cleanup();

        static void MemoryLogging(uint32_t,
                                  uintptr_t,
                                  uintptr_t,
                                  uintptr_t,
                                  uintptr_t,
                                  uint32_t);

        static void RecordMalloc(const uint64_t ptr, size_t bytes, int skip);
        static void RecordFree(const uint64_t ptr, int skip);
        
        static void ProcessSampleBuffer();

        static inline uint32_t interval_;
        static inline uint32_t process_period_ = 200; // ms
        static inline intptr_t period_ = 1000; // us
        static inline intptr_t bg_period_ = 1000; // us

    private:
        static void ThreadMain(uword parameters);

        static intptr_t CalculateBufferSize();

        static void ClearSamples();
        static void ProcessCompletedSamples();
        static void ProcessSample(MemorySample &sample, std::vector<MemRecord>& processed_buffer);
        
        static inline std::atomic<bool> initialized_ = false;
        static inline std::atomic<bool> s_main_running = false;
        static inline bool lite_mode_ = true;

        static inline MemorySampleBuffer *sample_buffer_ = nullptr;

        static inline Unfairlock profiler_lock_;
        
        static inline std::atomic<uint32_t> working_thread_;

        friend class MemorySampleBuffer;
    };

} // namespace btrace

#endif

#endif // BTRACE_MEMORY_PROFILER_H_
