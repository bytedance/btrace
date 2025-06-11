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

// Copyright (c) 2015, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include <cstddef>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_host.h>	  // NOLINT
#include <mach/mach_init.h>	  // NOLINT
#include <mach/mach_port.h>	  // NOLINT
#include <mach/mach_traps.h>  // NOLINT
#include <mach/task_info.h>	  // NOLINT
#include <mach/thread_act.h>  // NOLINT
#include <mach/thread_info.h> // NOLINT
#include <mach/vm_map.h>	  // NOLINT
#include <signal.h>			  // NOLINT
#include <sys/errno.h>		  // NOLINT
#include <sys/sysctl.h>		  // NOLINT
#include <sys/types.h>		  // NOLINT

#include <unordered_set>

#include "os_thread.hpp"
#include "utils.hpp"
#include "assert.hpp"
#include "profiler.hpp"
#include "event_loop.hpp"
#include "allocation.hpp"
#include "interrupter.hpp"

namespace btrace
{
	class ThreadStartData
	{
	public:
		ThreadStartData(const char *name,
						OSThread::ThreadStartFunction function,
						uword parameter)
			: name_(name), function_(function), parameter_(parameter) {}

		const char *name() const { return name_; }
		OSThread::ThreadStartFunction function() const { return function_; }
		uword parameter() const { return parameter_; }

	private:
		const char *name_;
		OSThread::ThreadStartFunction function_;
		uword parameter_;

		DISALLOW_COPY_AND_ASSIGN(ThreadStartData);
	};

	static void *ThreadStart(void *data_ptr)
	{
		ThreadStartData *data = reinterpret_cast<ThreadStartData *>(data_ptr);

		const char *name = data->name();
		OSThread::ThreadStartFunction function = data->function();
		uword parameter = data->parameter();
		delete data;

		char truncated_name[OSThread::kNameBufferSize];
		snprintf(truncated_name, ARRAY_SIZE(truncated_name), "%s", name);
		pthread_setname_np(truncated_name);

		OSThread *thread = OSThread::Current();

		if (thread != nullptr)
		{
			thread->set_name(truncated_name);
			// Call the supplied thread start function handing it its parameters.
			function(parameter);
		}

		return nullptr;
	}

	int OSThread::New(const char *name,
						ThreadStartFunction function,
						uword parameter)
	{
		pthread_attr_t attr;
		int result = pthread_attr_init(&attr);
		RETURN_ON_PTHREAD_FAILURE(result);

		result = pthread_attr_setstacksize(&attr, OSThread::GetMaxStackSize());
		RETURN_ON_PTHREAD_FAILURE(result);

		ThreadStartData *data = new ThreadStartData(name, function, parameter);

		pthread_t tid;
		result = pthread_create(&tid, &attr, ThreadStart, data);
		RETURN_ON_PTHREAD_FAILURE(result);

		result = pthread_attr_destroy(&attr);
		RETURN_ON_PTHREAD_FAILURE(result);

		return 0;
	}

	OSThread *OSThread::FindCurrent()
	{
		auto tid = GetCurrentThreadId();

        std::lock_guard ml(*thread_lock_);

        if (!started_)
        {
            return nullptr;
        }

        auto os_thread = GetThreadById(tid);

        if (os_thread)
        {
            OSThread::SetCurrent(os_thread);
		}

		return os_thread;
	}

    OSThread *OSThread::TryCurrent(bool is_main)
    {
        if (is_main) {
            return main_thread_;
        }
        
        auto os_thread = GetCurrentTLS();
        
        if (os_thread != nullptr) {
            return os_thread;
        }
        
        os_thread = FindCurrent();

        return os_thread;
    }

	ThreadLocalKey OSThread::CreateThreadLocal(ThreadDestructor destructor)
	{
		pthread_key_t key = kUnsetThreadLocalKey;
		int result = pthread_key_create(&key, destructor);
        ASSERT_PTHREAD_SUCCESS(result);
		ASSERT(key != kUnsetThreadLocalKey);
		return key;
	}

	void OSThread::DeleteThreadLocal(ThreadLocalKey key)
	{
		ASSERT(key != kUnsetThreadLocalKey);
		int result = pthread_key_delete(key);
        ASSERT_PTHREAD_SUCCESS(result);
	}

	void OSThread::SetRealTimePriority(uint32_t period, double guaranteed, double duty)
	{
		const pthread_t p_thread = join_id_;
		if (!p_thread)
		{
			return;
		}

		kern_return_t result;
		// Increase thread priority to real-time.
		// Please note that the thread_policy_set() calls may fail in
		// rare cases if the kernel decides the system is under heavy load
		// and is unable to handle boosting the thread priority.
		// In these cases we just return early and go on with life.
		// Make thread fixed priority.
		{
			thread_extended_policy_data_t policy;
			policy.timeshare = 0; // Set to 1 for a non-fixed thread.
			result = thread_policy_set(id_,
									   THREAD_EXTENDED_POLICY,
									   (thread_policy_t)&policy,
									   THREAD_EXTENDED_POLICY_COUNT);
			if (result != KERN_SUCCESS)
			{
				return;
			}
		}
		{
			// Set to relatively high priority.
			thread_precedence_policy_data_t precedence;
			precedence.importance = 63;
			result = thread_policy_set(id_,
									   THREAD_PRECEDENCE_POLICY,
									   (thread_policy_t)&precedence,
									   THREAD_PRECEDENCE_POLICY_COUNT);
			if (result != KERN_SUCCESS)
			{
				return;
			}
		}
		{
			// Most important, set real-time constraints.
			// Define the guaranteed and max fraction of time for the thread.
			// These "duty cycle" values can range from 0 to 1.  A value of 0.5
			// means the scheduler would give half the time to the thread.
			// These values have empirically been found to yield good behavior.
			// Good means that performance is high and other threads won't starve.
			mach_timebase_info_data_t timebase_info;
			mach_timebase_info(&timebase_info);

			const uint64_t NANOS_PER_MSEC = 1000000ULL;
			double clock2abs = ((double)timebase_info.denom / (double)timebase_info.numer) * NANOS_PER_MSEC;

			const double kTimeQuantum = period;
			// Time guaranteed each quantum.
			const double kGuaranteedDutyCycle = guaranteed;
			const double kTimeNeeded = kGuaranteedDutyCycle * kTimeQuantum;
			// Maximum time each quantum.
			const double kMaxDutyCycle = duty;
			const double kMaxTimeAllowed = kMaxDutyCycle * kTimeQuantum;

			thread_time_constraint_policy_data_t policy;
			policy.period = kTimeQuantum * clock2abs;
			policy.computation = (uint32_t)(kTimeNeeded * clock2abs);
			policy.constraint = (uint32_t)(kMaxTimeAllowed * clock2abs);
			policy.preemptible = 0;
			result = thread_policy_set(id_,
									   THREAD_TIME_CONSTRAINT_POLICY,
									   (thread_policy_t)&policy,
									   THREAD_TIME_CONSTRAINT_POLICY_COUNT);
			if (result != KERN_SUCCESS)
			{
			}
		}
	}

	intptr_t OSThread::GetMaxStackSize()
	{
		const int kStackSize = (128 * kWordSize * KB);
		return kStackSize;
	}

	ThreadId OSThread::GetCurrentThreadId()
	{
        return pthread_mach_thread_np(OSThread::PthreadSelf());
	}

    uint64_t OSThread::GetThreadUid(ThreadId id)
    {
        thread_identifier_info_data_t info;
        mach_msg_type_number_t info_count = THREAD_IDENTIFIER_INFO_COUNT;
        kern_return_t kr = thread_info(id, THREAD_IDENTIFIER_INFO,
                                     (thread_info_t)&info, &info_count);
        if(kr!=KERN_SUCCESS) {
            return 0;
        }
        
        return info.thread_id;
    }

	char *OSThread::GetThreadName(ThreadJoinId id)
	{
		char *name = static_cast<char *>(malloc(kNameBufferSize));
		pthread_getname_np(id, name, kNameBufferSize);
		return name;
	}

	void OSThread::Join(ThreadJoinId id)
	{
		int result = pthread_join(id, nullptr);
		ASSERT(result == 0);
	}

	void OSThread::Detach(ThreadJoinId id)
	{
		int result = pthread_detach(id);
		ASSERT(result == 0);
	}

	bool OSThread::Compare(ThreadId a, ThreadId b)
	{
		return a == b;
	}

	bool OSThread::GetStackBounds(ThreadJoinId id, uword *lower, uword *upper)
	{
        uword stack_base = reinterpret_cast<uword>(pthread_get_stackaddr_np(id));
        uword stack_limit = stack_base - pthread_get_stacksize_np(id);
        
        if (stack_base == 0 || stack_limit == 0 || stack_base < stack_limit)
        {
            return false;
        }
        
        *upper = stack_base;
        *lower = stack_limit;
        
		return true;
	}

	void OSThread::VisitThreads(ThreadVisitor *visitor)
	{
		if (visitor == nullptr)
		{
			return;
		}

		OSThreadIterator it;
		while (it.HasNext())
		{
			OSThread *t = it.Next();
			visitor->VisitThread(t);
		}
	}

	void OSThread::ConstructThreads()
	{
		if (OSThread::enable_thread_map_)
		{
			OSThread::ConstructThreadMap();
		}
		else
		{
			OSThread::ConstructThreadList();
		}
	}

	void OSThread::ConstructThreadMap()
	{
		thread_act_array_t threads;
		mach_msg_type_number_t thread_count;

		if (task_threads(mach_task_self(), &threads, &thread_count) != KERN_SUCCESS)
		{
			return;
		}

		std::unordered_set<thread_act_t> mach_id_set;
		for (int i = 0; i < thread_count; ++i)
		{
			ThreadId id = threads[i];
			mach_id_set.insert(id);
		}

		vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads), thread_count * sizeof(thread_act_t));
        
        std::lock_guard ml(*thread_lock_);

        if (!started_)
        {
            return;
        }

        for (auto it = thread_map_->begin(); it != thread_map_->end();)
        {
            ThreadId id = it->first;
            OSThread *os_thread = it->second;
            if (mach_id_set.count(id) == 0)
            {
                delete os_thread;
                it = thread_map_->erase(it);
            }
            else
            {
                ++it;
                mach_id_set.erase(id);
            }
        }

		for (const auto &id : mach_id_set)
		{
            ThreadJoinId pthread_id = pthread_from_mach_thread_np(id);
            
            if (pthread_id == OSThread::kInvalidThreadJoinId)
            {
                continue;
            }
            
			OSThread *os_thread = new OSThread(id);
			AddThreadToMap(os_thread);
		}
	}

	void OSThread::ConstructThreadList()
	{
		thread_act_array_t threads;
		mach_msg_type_number_t thread_count;

		if (task_threads(mach_task_self(), &threads, &thread_count) != KERN_SUCCESS)
		{
			return;
		}

		if (!OSThread::enable_pthread_hook_)
		{
			ResetThreadListState();
		}
        
        std::lock_guard ml(*thread_lock_);
        
        if (!started_) {
            return;
        }

		for (int i = 0; i < thread_count; ++i)
		{
			ThreadId id = threads[i];
			ThreadJoinId pthread_id = pthread_from_mach_thread_np(id);
			if (pthread_id == OSThread::kInvalidThreadJoinId)
			{
				continue;
			}
            
			OSThread *os_thread = OSThread::GetThreadById(id);

			if (!os_thread)
			{
				os_thread = new OSThread(id);
				AddThreadToList(os_thread);
			}
			else
			{
				os_thread->alive_ = true;
			}
		}

		vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads), thread_count * sizeof(thread_act_t));
	}

	OSThread::OSThread()
	{
		join_id_ = OSThread::PthreadSelf();
		id_ = pthread_mach_thread_np(join_id_);
		alive_ = InitThreadStack();
	}

	OSThread::OSThread(ThreadId id): id_(id)
	{
		join_id_ = pthread_from_mach_thread_np(id);
		alive_ = InitThreadStack();
	}

	bool OSThread::InitThreadStack()
	{
		// Try to get accurate stack bounds from pthreads, etc.
		if (!GetStackBounds(join_id_, &stack_limit_, &stack_base_))
		{
			Utils::PrintErr("Failed to retrieve stack bounds");
			return false;
		}

		return true;
	}

	OSThread *OSThread::CreateOSThread(ThreadId tid)
	{
		if (tid == 0)
		{
			tid = GetCurrentThreadId();
		}

		// must before lock_guard to avoid dead lock
		auto os_thread = new OSThread(tid);

		std::lock_guard ml(*thread_lock_);
            
		if (!started_)
		{
			delete os_thread;
			return nullptr;
		}
        
		OSThread::SetCurrent(os_thread);
        
		if (OSThread::enable_thread_map_)
		{
			AddThreadToMap(os_thread);
		}
		else
		{
			AddThreadToList(os_thread);
		}
        
		return os_thread;
	}

	OSThread::~OSThread()
	{
		//        Utils::Print("delete os_thread, id: %u", id_);
		free(name_);
	}

	void OSThread::set_name(const char *name)
	{
		// Clear the old thread name.
		if (name_ != nullptr)
		{
			free(name_);
			name_ = nullptr;
		}
		ASSERT(OSThread::TryCurrent(false) == this);
		ASSERT(name != nullptr);
		name_ = strdup(name);
	}

    bool OSThread::apple_thread()
    {
        if (apple_thread_ == -1) {
            const char *thread_name = name();
            const char *prefix = "com.apple.";
            int res = 1;
            
            if (thread_name != nullptr) {
                res = strncmp(thread_name, prefix, strlen(prefix));
            }

            if (res == 0) {
                apple_thread_ = 1;
            } else {
                apple_thread_ = 0;
            }
        }
        return apple_thread_ == 1;
    }

	void OSThread::DisableInterrupts()
	{
        ASSERT(OSThread::TryCurrent(false) == this);
		thread_interrupt_disabled_.fetch_add(1u);
	}

	void OSThread::EnableInterrupts()
	{
        ASSERT(OSThread::TryCurrent(false) == this);

		uintptr_t old = thread_interrupt_disabled_.fetch_sub(1u);

        ASSERT(old != 0);
	}

	bool OSThread::InterruptsEnabled()
	{
		return thread_interrupt_disabled_ == 0;
	}

	//	static void DeleteThread(void *thread)
	//	{
	//		delete reinterpret_cast<OSThread *>(thread);
	//	}

	void OSThread::Init()
    {
        if (thread_lock_ == nullptr) {
            // do not delete thread_lock_
            thread_lock_ = new std::recursive_mutex();
        }
        
        if (thread_logging_ == kUnsetThreadLocalKey)
        {
            // do not delete thread_logging_
            thread_logging_ = OSThread::CreateThreadLocal(nullptr);
        }
        
        started_ = false;
    }

    void OSThread::Start()
    {
        {
            std::lock_guard ml(*thread_lock_);
            
            if (started_) {
                return;
            }
            
            // Create the thread local key.
            if (thread_key_ == kUnsetThreadLocalKey)
            {
                //            thread_key_ = CreateThreadLocal(DeleteThread);
                thread_key_ = CreateThreadLocal(nullptr);
            }
            ASSERT(thread_key_ != kUnsetThreadLocalKey);
            
            if (OSThread::enable_thread_map_ && thread_map_ == nullptr)
            {
                thread_map_ = new std::unordered_map<ThreadId, OSThread *>();
            }

            main_logging_ = false;
            started_ = true;
        }

		// Create a new OSThread structure and set it as the TLS.
		OSThread *os_thread = CreateOSThread();
		ASSERT(os_thread != nullptr);
		os_thread->set_name("BTrace_Initialize");

		if (OSThread::enable_pthread_hook_)
		{
			std::call_once(pthread_hook_flag_, InstallPthreadHook);
		}
        
        if (!OSThread::enable_pthread_hook_) {
            EventLoop::Register(100, OSThread::ConstructThreads);
        }

		ConstructThreads();
		InitMainThread();
	}

    void OSThread::InitMainThread() {
        OSThread *thread = nullptr;
        
        OSThreadIterator it;
        while (it.HasNext())
        {
            OSThread *t = it.Next();
            if (thread == nullptr || t->id() < thread->id())
            {
                thread = t;
            }
        }
        main_thread_ = thread;
        main_id_ = main_thread_->id_;
        main_join_id_ = main_thread_->join_id_;
    }

	void OSThread::Stop()
	{
		std::lock_guard ml(*thread_lock_);
        
        if (!started_) {
            return;
        }
        
		if (OSThread::enable_thread_map_)
		{
			for (auto it = thread_map_->begin(); it != thread_map_->end(); ++it)
			{
				OSThread *os_thread = it->second;
				delete os_thread;
			}

			delete thread_map_;
			thread_map_ = nullptr;
		}
		else
		{
			OSThread *current = thread_list_head_;
			OSThread *next = nullptr;

			while (current != nullptr)
			{
				next = current->thread_list_next_;
				delete current;
				current = next;
			}

			thread_list_head_ = nullptr;
		}
		// Delete the thread local key.
		if (thread_key_ != kUnsetThreadLocalKey)
		{
			DeleteThreadLocal(thread_key_);
			thread_key_ = kUnsetThreadLocalKey;
		}
        
        main_thread_ = nullptr;
		started_ = false;
	}

	void OSThread::InstallPthreadHook()
	{
		prev_pthread_hook_ = pthread_introspection_hook_install(PthreadIntrospectionHook);
	}

	void OSThread::PthreadIntrospectionHook(unsigned int event, pthread_t thread, void *addr, size_t size)
	{
        mach_port_t mach_id;
        
        if (!started_)
        {
            goto end;
        }

		mach_id = pthread_mach_thread_np(thread);
		switch (event)
		{
		case PTHREAD_INTROSPECTION_THREAD_START:
		{
			// Create new OSThread object and set as TLS for new thread.
			CreateOSThread(mach_id);
			break;
		}
		case PTHREAD_INTROSPECTION_THREAD_TERMINATE:
		{
            OSThread::DisableLogging(mach_id==main_id_);
            
			if (OSThread::enable_thread_map_)
			{
				RemoveThreadFromMap(mach_id);
			}
			else
			{
				RemoveThreadFromList(mach_id);
			}
			break;
		}
		default:
			break;
		}

    end:
        // callback prev hook
        if (prev_pthread_hook_)
        {
            prev_pthread_hook_(event, thread, addr, size);
        }
	}

	void OSThread::RemoveThreadFromList(ThreadId id)
	{
        if (!started_)
        {
            return;
        }
        
		std::lock_guard ml(*thread_lock_);
        
        if (!started_)
        {
            return;
        }
        
		OSThread *current = thread_list_head_;
		OSThread *previous = nullptr;

		// Scan across list and remove |thread|.
		while (current != nullptr)
		{
			if (current->id_ == id)
			{
				// We found |thread|, remove from list.
				if (previous == nullptr)
				{
					thread_list_head_ = current->thread_list_next_;
				}
				else
				{
					previous->thread_list_next_ = current->thread_list_next_;
				}
				current->thread_list_next_ = nullptr;
                delete current;
				break;
			}
			previous = current;
			current = current->thread_list_next_;
		}
	}

	void OSThread::RemoveThreadFromMap(ThreadId id)
	{
        if (!started_)
        {
            return;
        }
        
		std::lock_guard ml(*thread_lock_);
        
		if (!started_)
		{
			return;
		}
        
        auto it = thread_map_->find(id);
        if (it != thread_map_->end()) {
            OSThread *current = it->second;
            delete current;
            thread_map_->erase(it);
        }
	}

	OSThread *OSThread::GetThreadById(ThreadId id)
	{
		if (id == OSThread::kInvalidThreadId)
		{
			return nullptr;
		}
        
        if (OSThread::enable_thread_map_) 
        {
            auto it = thread_map_->find(id);
            if (it != thread_map_->end())
            {
                return it->second;
            }
        }
        else
        {
            OSThread *current = thread_list_head_;
            OSThread *next = nullptr;

            while (current != nullptr)
            {
                if (current->id() == id)
                {
                    return current;
                }
                next = current->thread_list_next_;
                current = next;
            }
        }

		return nullptr;
	}

    void OSThread::AddThreadToMap(OSThread *thread)
    {
        ASSERT(thread != nullptr);
        thread_map_->emplace(thread->id_, thread);
    }

	void OSThread::AddThreadToList(OSThread *thread)
	{
		ASSERT(thread != nullptr);
		ASSERT(thread->thread_list_next_ == nullptr);

		// Insert at head of list.
		thread->thread_list_next_ = thread_list_head_;
		thread_list_head_ = thread;
	}

	void OSThread::ResetThreadListState()
	{
		OSThreadIterator it;
		while (it.HasNext())
		{
			OSThread *t = it.Next();
			t->alive_ = false;
		}
	}

	bool OSThread::BasicInfo(thread_basic_info_data_t *info)
	{
        mach_msg_type_number_t thread_info_count = THREAD_BASIC_INFO_COUNT;
		kern_return_t res = thread_info(id_, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(info), &thread_info_count);
		if (unlikely((res != KERN_SUCCESS)))
		{
			return false;
		}
		return true;
	}

	bool OSThread::Active(thread_basic_info_data_t &info)
	{
		bool result = false;

		result = (info.run_state == TH_STATE_RUNNING && (info.flags & TH_FLAGS_IDLE) == 0);
		//    result = (info.run_state == TH_STATE_RUNNING);

		return result;
	}

    uint64_t OSThread::update_cpu_time() {
		uint64_t result = 0;
        
        ASSERT(GetCurrentThreadId() == id_);
        
        struct timespec tp;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
        result = tp.tv_sec * kMicrosPerSec + tp.tv_nsec / kNanosPerMicro;
        
        cpu_time_ = result;

        return result;
    }

	OSThreadIterator::OSThreadIterator()
	{
		// Lock the thread list while iterating.
        OSThread::thread_lock_->lock();
		if (OSThread::enable_thread_map_)
		{
			it = OSThread::thread_map_->begin();
		}
		else
		{
			next_ = OSThread::thread_list_head_;
		}
	}

	OSThreadIterator::~OSThreadIterator()
	{
		// Unlock the thread list when done.
        OSThread::thread_lock_->unlock();
	}

	bool OSThreadIterator::HasNext() const
	{
		if (OSThread::enable_thread_map_)
		{
			return it != OSThread::thread_map_->end();
		}
		return next_ != nullptr;
	}

	OSThread *OSThreadIterator::Next()
	{
		OSThread *current;
		if (OSThread::enable_thread_map_)
		{
			current = it->second;
			++it;
		}
		else
		{
			current = next_;
			next_ = next_->thread_list_next_;
		}
		return current;
	}

} // namespace btrace
