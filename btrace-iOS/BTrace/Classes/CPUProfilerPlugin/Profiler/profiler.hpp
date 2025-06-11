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
 *
 * Includes work from dart-lang/sdk (https://github.com/dart-lang/sdk)
 * with modifications.
 */

// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef BTRACE_PROFILER_H_
#define BTRACE_PROFILER_H_

#ifdef __cplusplus

#include <atomic>

#include "globals.hpp"
#include "interrupter.hpp"
#include "memory.hpp"
#include "zone_vector.hpp"
#include "zone.hpp"
#include "ring_buffer.hpp"

// Profiler sampling and stack walking support.、
namespace btrace
{
	class Profile;

    typedef void (*ProfileCallback)(btrace::Profile *profile);

	// Forward declarations.
	class ProcessedSample;
	class ProcessedSampleBuffer;

	class Sample;
	class SampleBuffer;
    class CPURecordBuffer;

	typedef _STRUCT_MCONTEXT ThreadContext;

	typedef struct
	{
		ThreadContext registers;
		uint32_t frameCount;
#if CLS_COMPACT_UNWINDING_SUPPORTED
		CompactUnwindContext compactUnwindState;
#endif
		uintptr_t lastFramePC;
	} UnwindContext;

	// The model for a profile. Most of the model is zone allocated, therefore
	// a zone must be created that lives longer than this object.
	class Profile : public ValueObject
	{
	public:
		Profile();

		void Build(SampleBuffer *sample_buffer);

		// After building:
		intptr_t sample_count() const { return sample_count_; }
		ProcessedSample *SampleAt(intptr_t index);

	private:
		Zone *zone_;
		ProcessedSampleBuffer *samples_;

		intptr_t sample_count_;

		friend class ProfileBuilder;
	};

	class SampleVisitor : public ValueObject
	{
	public:
		explicit SampleVisitor() : visited_(0) {}
		virtual ~SampleVisitor() {}

		virtual void VisitSample(Sample *sample) = 0;

		virtual void Reset() { visited_ = 0; }

		intptr_t visited() const { return visited_; }

		void IncrementVisited() { visited_++; }

	private:
		intptr_t visited_;

		//		DISALLOW_IMPLICIT_CONSTRUCTORS(SampleVisitor);
	};

	// Each Sample holds a stack trace from an isolate.
	class Sample
	{
	public:
        static constexpr int kPCArraySize = 32;
#if DEBUG || INHOUSE_TARGET || TEST_MODE
    static constexpr uintptr_t kAvgDepth = 32;
    static constexpr uintptr_t kMaxThreads = 128;
#else
    static constexpr uintptr_t kAvgDepth = 16;
    static constexpr uintptr_t kMaxThreads = 128;
#endif
        
		Sample() = default;
        
        ~Sample() = default;

        Sample(int64_t timestamp, ThreadId tid, uint32_t cpu_time,
               uint32_t alloc_size, uint32_t alloc_count);

		// Thread sample was taken on.
		ThreadId tid() const { return data_.tid; }
        
        uint32_t size() const { return size_; }
        
        void set_size(uint32_t size) { size_ = size; }

		uint32_t cpu_time() const { return data_.cpu_time; }

		// Timestamp sample was taken at.
		int64_t timestamp() const { return data_.timestamp; }

		// Top most pc.
		uword pc() const { return At(0); }

		// Get stack trace entry.
		uword At(intptr_t i) const
		{
			ASSERT(i >= 0);
			ASSERT(i < kMaxStackDepth);
			return pcs_[i];
		}

		// Set stack trace entry.
		void SetAt(intptr_t i, uword pc)
		{
			ASSERT(i >= 0);
			ASSERT(i < kMaxStackDepth);
			pcs_[i] = pc;
            ++size_;
		}
        
        uword *SetPCArray(uword *buffer) { return pcs_ = buffer; }

        uword *GetPCArray() { return &pcs_[0]; }
        
        ProcessedSampleBuffer *BuildProcessedSampleBuffer(
            ProcessedSampleBuffer *buffer = nullptr);
        
        ProcessedSample *BuildProcessedSample();

	private:
        struct SampleData {
            int64_t timestamp;
            ThreadId tid;
            uint32_t cpu_time;
            uint32_t alloc_size;
            uint32_t alloc_count;
        } data_;
        uword *pcs_ = nullptr;
        uint32_t size_;
		DISALLOW_COPY_AND_ASSIGN(Sample);
        
        friend class SampleBuffer;
	};

	class SampleBuffer: public RingBuffer
	{
	public:
        SampleBuffer(uintptr_t size) : RingBuffer(size) {}
        
        ProcessedSampleBuffer *BuildProcessedSampleBuffer(ProcessedSampleBuffer *buffer=nullptr);
        
        bool PutSample(Sample &sample);
        
        bool GetSample(Sample *sample);

	private:
        bool GetSampleFromBuffer(RingBuffer::Buffer *buffer, Sample *out);
		DISALLOW_COPY_AND_ASSIGN(SampleBuffer);
	};

	class Profiler : public AllStatic
	{
	public:
		/*
		 * @param profile_period
		 * Unit: millisecond;
		 * @param duration
		 * Unit: sencond;
		 * @param main_thread_only
		 * Only record main thread's backtrace
		 */
		static void EnableProfiler(intptr_t profile_period = 1, intptr_t profile_bg_period = 1,
                                   bool main_thread_only = false, bool active_thread_only = true,
                                   bool fast_unwind = true);
		static void DisableProfiler();

		static void Init();
		static void Cleanup();

		static void SetSampleDepth(intptr_t depth);
		static void SetSamplePeriod(intptr_t period);
		static void SetProfileDuration(intptr_t duration);
		static void SetProfileCallback(ProfileCallback callback);
        
        static SampleBuffer *sample_buffer()
        {
            return sample_buffer_;
        }
        
        static void set_sample_buffer(SampleBuffer *buffer);

		// It is very critical that the implementation of SampleThread does not do
		// any of the following:
		//   * Accessing TLS -- Because on Mac the callback will be running in a
        //                      different thread.
		//   * Allocating memory -- Because this takes locks which may already be
		//                          held, resulting in a dead lock.
		//   * Taking a lock -- See above.
		static void SampleThread(OSThread *thread, ThreadContext &state);

		static inline intptr_t Size();

        static void ProcessSampleBuffer();
		static void ProcessCompletedSamples();

		static void ClearSamples();

	private:
		// Calculates the sample buffer capacity.
		// The capacity is based on the sample rate, maximum
		// sample stack depth, maximum threads, and the sample process rate.
		static intptr_t CalculateCapacity();
        
        static inline std::mutex *profiler_lock_;
        
        static void ThreadMain(uword parameters);
        
        static void UpdateThreadState();

        static inline Monitor *monitor_ = nullptr;
		static inline std::atomic<bool> initialized_ = false;
        static inline SampleBuffer *sample_buffer_ = nullptr;
        
        static inline intptr_t profile_period_ = 1;
        static inline intptr_t profile_bg_period_ = 1;
        static inline bool main_thread_only_ = false;
        static inline bool active_thread_only_ = true;
        static inline bool fast_unwind_ = true;
        static constexpr int process_period_ = 200; // unit: ms
        static constexpr intptr_t kMainUpdatePeriod = 30;
        static constexpr intptr_t kBgUpdatePeriod = 300;

        friend class OSThread;
        friend class OSThreadIterator;
		friend class ThreadInterrupter;
		friend class ProfilerStackWalker;
        friend class ThreadInterrupterVisitor;
	};

	// A |ProcessedSample| is a combination of 1 (or more) |Sample|(s) that have
	// been merged into a logical sample. The raw data may have been processed to
	// improve the quality of the stack trace.
	class ProcessedSample : public ZoneAllocated
	{
	public:
		ProcessedSample();

		// Timestamp sample was taken at.
		int64_t timestamp() const { return timestamp_; }
		void set_timestamp(int64_t timestamp) { timestamp_ = timestamp; }

		ThreadId tid() const { return tid_; }
		void set_tid(ThreadId tid) { tid_ = tid; }

		uint32_t cpu_time() const { return cpu_time_; }
		void set_cpu_time(uint32_t cpu_time) { cpu_time_ = cpu_time; }
        
        uint32_t stack_id() const { return stack_id_; }
        void set_stack_id(uint32_t stack_id) { stack_id_ = stack_id; }
        
        uint32_t alloc_size() const { return alloc_size_; }
        void set_alloc_size(uint32_t alloc_size) { alloc_size_ = alloc_size; }
        
        uint32_t alloc_count() const { return alloc_count_; }
        void set_alloc_count(uint32_t alloc_count) { alloc_count_ = alloc_count; }

	private:
		int64_t timestamp_;
		ThreadId tid_;
		uint32_t cpu_time_;
        uint32_t stack_id_;
        uint32_t alloc_size_;
        uint32_t alloc_count_;

		friend class SampleBuffer;
		DISALLOW_COPY_AND_ASSIGN(ProcessedSample);
	};

	// A collection of |ProcessedSample|s.
	class ProcessedSampleBuffer : public ZoneAllocated
	{
	public:
		ProcessedSampleBuffer();

		void Add(ProcessedSample *sample) { samples_.Add(sample); }

		intptr_t length() const { return samples_.length(); }

		ProcessedSample *At(intptr_t index) { return samples_.At(index); }

	private:
		ZoneVector<ProcessedSample *> samples_;

		DISALLOW_COPY_AND_ASSIGN(ProcessedSampleBuffer);
	};
} // namespace btrace

#endif

#endif // BTRACE_PROFILER_H_
