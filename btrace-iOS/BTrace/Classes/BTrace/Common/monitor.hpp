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

// Copyright (c) 2024, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef BTRACE_MONITOR_H_
#define BTRACE_MONITOR_H_

#include <os/lock.h>

#ifdef __cplusplus

#include "assert.hpp"
#include "allocation.hpp"
#include "globals.hpp"
#include "os_thread.hpp"

namespace btrace
{
    class Unfairlock {
    public:
        Unfairlock() = default;
        
        Unfairlock(const Unfairlock&) = delete;
        Unfairlock& operator=(const Unfairlock&) = delete;
        
        ~Unfairlock() = default;
        
        void lock() {
            os_unfair_lock_lock(&lock_);
        }
        
        bool try_lock() {
            return os_unfair_lock_trylock(&lock_);
        }
        
        void unlock() {
            os_unfair_lock_unlock(&lock_);
        }
    private:
        os_unfair_lock lock_ = OS_UNFAIR_LOCK_INIT;
    };
    
    class ScopedUnfairlock {
    public:
        ScopedUnfairlock(Unfairlock &lock) : lock_(lock) {
            lock_.lock();
        }
        
        ~ScopedUnfairlock() { lock_.unlock(); }
        
    private:
        ScopedUnfairlock(const ScopedUnfairlock&) = delete;
        ScopedUnfairlock& operator=(const ScopedUnfairlock&) = delete;
        
        ScopedUnfairlock(ScopedUnfairlock&& other) = delete;
        
        ScopedUnfairlock& operator=(ScopedUnfairlock&& other) = delete;
        
        Unfairlock &lock_;
    };

	class MonitorData
	{
	private:
		MonitorData() {}
		~MonitorData() {}

		pthread_mutex_t *mutex() { return &mutex_; }
		pthread_cond_t *cond() { return &cond_; }

		pthread_mutex_t mutex_;
		pthread_cond_t cond_;

		friend class Monitor;

		DISALLOW_ALLOCATION();
		DISALLOW_COPY_AND_ASSIGN(MonitorData);
	};

	class Monitor
	{
	public:
		enum WaitResult
		{
			kNotified,
			kTimedOut
		};

		static constexpr int64_t kNoTimeout = 0;

		Monitor();
		~Monitor();

		bool IsOwnedByCurrentThread() const;

		bool TryEnter(); // Returns false if lock is busy and locking failed.
		void Enter();
		void Exit();

		// Wait for notification or timeout.
		WaitResult Wait(int64_t millis);
		WaitResult WaitMicros(int64_t micros);

		// Notify waiting threads.
		void Notify();
		void NotifyAll();

    private:
		MonitorData data_; // OS-specific data.
#if defined(DEBUG)
		ThreadId owner_;
#endif // defined(DEBUG)

		friend class MonitorLocker;
		DISALLOW_COPY_AND_ASSIGN(Monitor);
	};

	class MonitorLocker : public ValueObject
	{
	public:
		explicit MonitorLocker(Monitor *monitor)
			: monitor_(monitor)
		{
			ASSERT(monitor != nullptr);
			monitor_->Enter();
		}

		virtual ~MonitorLocker()
		{
			monitor_->Exit();
		}

		void Enter() const
		{
			monitor_->Enter();
		}
		void Exit() const
		{
			monitor_->Exit();
		}

		Monitor::WaitResult Wait(int64_t millis = Monitor::kNoTimeout)
		{
			return monitor_->Wait(millis);
		}

		Monitor::WaitResult WaitMicros(int64_t micros = Monitor::kNoTimeout)
		{
			return monitor_->WaitMicros(micros);
		}

		void Notify() { monitor_->Notify(); }

		void NotifyAll() { monitor_->NotifyAll(); }

	private:
		Monitor *const monitor_;

		DISALLOW_COPY_AND_ASSIGN(MonitorLocker);
	};
} // namespace btrace

#endif

#endif // BTRACE_MONITOR_H_
