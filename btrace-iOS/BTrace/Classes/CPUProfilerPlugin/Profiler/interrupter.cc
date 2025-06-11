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

#include <thread>
#include <chrono>
#include <assert.h>			  // NOLINT
#include <errno.h>			  // NOLINT
#include <mach/kern_return.h> // NOLINT
#include <mach/mach.h>		  // NOLINT
#include <mach/thread_act.h>  // NOLINT
#include <stdbool.h>		  // NOLINT
#include <sys/sysctl.h>		  // NOLINT
#include <sys/types.h>		  // NOLINT
#include <unistd.h>			  // NOLINT

#include "utils.hpp"
#include "globals.hpp"
#include "profiler.hpp"
#include "interrupter.hpp"

#if defined(HOST_ARCH_X64)
#define THREAD_STATE_FLAVOR x86_THREAD_STATE64
#define THREAD_STATE_FLAVOR_SIZE x86_THREAD_STATE64_COUNT
#elif defined(HOST_ARCH_ARM64)
#define THREAD_STATE_FLAVOR ARM_THREAD_STATE64
#define THREAD_STATE_FLAVOR_SIZE ARM_THREAD_STATE64_COUNT
#else
#error "Unsupported architecture."
#endif // HOST_ARCH_...

namespace btrace
{
	// Notes:
	//
	// The ThreadInterrupter interrupts all threads actively once
	// per interrupt period (default is 1 millisecond).
	// thread interrupter is forbidden from:
	//   * Accessing TLS.
	//   * Allocating memory.
	//   * Taking a lock.
	//   * Printing
	//
	// The ThreadInterrupter has a single monitor (monitor_). This monitor is used
	// to synchronize startup, shutdown, and waking up from a deep sleep.

	class ThreadInterrupterVisitor : public ThreadVisitor
	{
	public:
		ThreadInterrupterVisitor() = default;
		virtual ~ThreadInterrupterVisitor() = default;

		void VisitThread(OSThread *thread)
		{
			if (!thread->InterruptsEnabled())
				return;

			if (!thread->alive())
				return;
            
            OSThread *main_thread = OSThread::MainThread();

            if (unlikely(Profiler::main_thread_only_ && thread != main_thread))
            {
                return;
            }

			ThreadJoinId pthread_id = thread->joinId();
			if (pthread_id == OSThread::kInvalidThreadJoinId)
				return;

			if (pthread_id == ThreadInterrupter::interrupter_thread_id_)
				return;
            
            int64_t access_time = thread->access_time();
            int64_t diff_time = curr_time_ - access_time;
            intptr_t period = ThreadInterrupter::interrupt_bg_period_;
                
            if unlikely(thread == main_thread) {
                period = ThreadInterrupter::interrupt_period_;
            }
            
            if (diff_time < period) {
                return;
            }

			ThreadInterrupter::InterruptThread(thread);
		}
        
        void set_curr_time(uint64_t curr_time) { curr_time_ = curr_time; }
    private:
        uint64_t curr_time_;
	};

	void ThreadInterrupter::Init()
	{
		ASSERT(!initialized_);
		if (monitor_ == nullptr)
		{
			monitor_ = new Monitor();
		}
		ASSERT(monitor_ != nullptr);
		initialized_ = true;
		shutdown_ = false;
	}

	void ThreadInterrupter::Startup()
	{
		ASSERT(initialized_);
		ASSERT(interrupter_thread_id_ == OSThread::kInvalidThreadJoinId);
		{
			MonitorLocker startup_ml(monitor_);
			OSThread::New("BTrace ThreadInterrupter", ThreadMain, 0);
			while (!thread_running_)
			{
				startup_ml.Wait(1);
			}
		}
		ASSERT(interrupter_thread_id_ != OSThread::kInvalidThreadJoinId);
	}

	void ThreadInterrupter::Cleanup()
	{
		{
			MonitorLocker shutdown_ml(monitor_);
			if (shutdown_)
			{
				// Already shutdown.
				return;
			}
			shutdown_ = true;
			// Notify.
			shutdown_ml.Notify();
			ASSERT(initialized_);
		}

		// Join the thread.
		if (interrupter_thread_id_)
		{
            OSThread *thread = OSThread::Current();
			if (!thread || thread->joinId() != interrupter_thread_id_)
			{
				OSThread::Join(interrupter_thread_id_);
			}
			else
			{
				OSThread::Detach(interrupter_thread_id_);
			}
		}

		interrupter_thread_id_ = OSThread::kInvalidThreadJoinId;
		delete monitor_;
		monitor_ = nullptr;
		initialized_ = false;
	}

	// Delay between interrupts.
	void ThreadInterrupter::SetPeriod(intptr_t period, intptr_t bg_period)
	{
		ASSERT(period > 0);
		interrupt_period_ = period * kMicrosPerMilli;
        interrupt_bg_period_ = bg_period * kMicrosPerMilli;
	}

	void ThreadInterrupter::ThreadMain(uword parameters)
	{
		if (!initialized_)
		{
			return;
		}
		
		OSThread::DisableLogging(false);

		{
			// Signal to main thread we are ready.
			MonitorLocker startup_ml(monitor_);
			OSThread *os_thread = OSThread::Current();
			ASSERT(os_thread != nullptr);
			os_thread->DisableInterrupts();
			os_thread->SetRealTimePriority();
			interrupter_thread_id_ = os_thread->joinId();
			thread_running_ = true;
			startup_ml.Notify();
		}
		{
			interrupted_thread_count_ = 0;
			ThreadInterrupterVisitor visitor;

            uint64_t count = 0;
            uint64_t period_ticks = Utils::MicrosToMachTicks(interrupt_period_);
            uint64_t prev_ticks = Utils::MachTicksNow();
            while (!shutdown_)
            {
                count += 1;
                if (unlikely(shutdown_))
                {
                    break;
                }
                // Reset count before interrupting any threads.
                interrupted_thread_count_ = 0;
                visitor.set_curr_time(Utils::GetCurrMachMicros());
                OSThread::VisitThreads(&visitor);
                
                int64_t now = Utils::MachTicksNow();
                Utils::WaitUntil(now+period_ticks);
            }
			Utils::Print("thread interrupter loop count: %d\n", count);
		}
		thread_running_ = false;
	}

	class ThreadInterrupterMacOS
	{
	public:
		explicit ThreadInterrupterMacOS(OSThread *os_thread) : os_thread_(os_thread)
		{
			ASSERT(os_thread != nullptr);

			mach_thread_ = os_thread->id();

			ASSERT(mach_thread_ != OSThread::kInvalidThreadId);
			res = thread_suspend(mach_thread_);
		}

		void CollectSample()
		{
			if (res != KERN_SUCCESS)
			{
				return;
			}

			auto count = static_cast<mach_msg_type_number_t>(THREAD_STATE_FLAVOR_SIZE);
			ThreadContext context;

			kern_return_t res = thread_get_state(mach_thread_, THREAD_STATE_FLAVOR,
                                                 (thread_state_t)(&(context.__ss)), &count);

			if (res != KERN_SUCCESS)
				return;

			Profiler::SampleThread(os_thread_, context);
		}

		~ThreadInterrupterMacOS()
		{
			if (res != KERN_SUCCESS)
			{
				return;
			}

			res = thread_resume(mach_thread_);
		}

	private:
		kern_return_t res;
		OSThread *os_thread_;
		mach_port_t mach_thread_;
	};

	void ThreadInterrupter::InterruptThread(OSThread *os_thread)
	{
        
        if (likely(os_thread->has_sampled()) &&
            Profiler::active_thread_only_ &&
            os_thread->cpu_usage() <= cpu_usage_threshold_) {
            return;
        }

        thread_basic_info_data_t info;
        os_thread->BasicInfo(&info);
        bool active = OSThread::Active(info);
        int64_t cpu_time = OSThread::CPUTime(info);
        int64_t prev_cpu_time = os_thread->cpu_time();
        int64_t diff_cpu_time = cpu_time - prev_cpu_time;
        int64_t period = interrupt_bg_period_;
                
        if unlikely(os_thread == OSThread::MainThread()) {
            period = interrupt_period_;
        }
                
        int64_t cpu_time_period = cpu_time_threshold_ * period / kMicrosPerMilli;
            
        if (likely(os_thread->has_sampled()) &&
            Profiler::active_thread_only_ &&
            diff_cpu_time < cpu_time_period &&
            !active)
        {
            return;
        }
        
        os_thread->set_cpu_time(cpu_time);

		interrupted_thread_count_++;
		ThreadInterrupterMacOS interrupter(os_thread);

		interrupter.CollectSample();
	}

} // namespace btrace
