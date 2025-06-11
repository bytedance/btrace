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

#include "monitor.hpp"

namespace btrace
{
	Monitor::Monitor()
	{
		pthread_mutexattr_t attr;
		int result = pthread_mutexattr_init(&attr);
        ASSERT_PTHREAD_SUCCESS(result);

#if defined(DEBUG)
		result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
        ASSERT_PTHREAD_SUCCESS(result);
#endif // defined(DEBUG)

		result = pthread_mutex_init(data_.mutex(), &attr);
        ASSERT_PTHREAD_SUCCESS(result);

		result = pthread_mutexattr_destroy(&attr);
        ASSERT_PTHREAD_SUCCESS(result);

		result = pthread_cond_init(data_.cond(), nullptr);
        ASSERT_PTHREAD_SUCCESS(result);

#if defined(DEBUG)
		// When running with assertions enabled we track the owner.
		owner_ = OSThread::kInvalidThreadId;
#endif // defined(DEBUG)
	}

	Monitor::~Monitor()
	{
#if defined(DEBUG)
		// When running with assertions enabled we track the owner.
		ASSERT(owner_ == OSThread::kInvalidThreadId);
#endif // defined(DEBUG)

		int result = pthread_mutex_destroy(data_.mutex());
        ASSERT_PTHREAD_SUCCESS(result);

		result = pthread_cond_destroy(data_.cond());
        ASSERT_PTHREAD_SUCCESS(result);
	}

	bool Monitor::TryEnter()
	{
		int result = pthread_mutex_trylock(data_.mutex());
		// Return false if the lock is busy and locking failed.
		if ((result == EBUSY) || (result == EDEADLK))
		{
			return false;
		}
		ASSERT_PTHREAD_SUCCESS(result); // Verify no other errors.
#if defined(DEBUG)
		// When running with assertions enabled we track the owner.
		ASSERT(owner_ == OSThread::kInvalidThreadId);
		owner_ = OSThread::GetCurrentThreadId();
#endif // defined(DEBUG)
		return true;
	}

	void Monitor::Enter()
	{
		int result = pthread_mutex_lock(data_.mutex());
        ASSERT_PTHREAD_SUCCESS(result);

#if defined(DEBUG)
		// When running with assertions enabled we track the owner.
		ASSERT(owner_ == OSThread::kInvalidThreadId);
		owner_ = OSThread::GetCurrentThreadId();
#endif // defined(DEBUG)
	}

	void Monitor::Exit()
	{
#if defined(DEBUG)
		// When running with assertions enabled we track the owner.
		ASSERT(IsOwnedByCurrentThread());
		owner_ = OSThread::kInvalidThreadId;
#endif // defined(DEBUG)

		int result = pthread_mutex_unlock(data_.mutex());
        ASSERT_PTHREAD_SUCCESS(result);
	}

	Monitor::WaitResult Monitor::Wait(int64_t millis)
	{
		return WaitMicros(millis * kMicrosPerMilli);
	}

	Monitor::WaitResult Monitor::WaitMicros(int64_t micros)
	{
#if defined(DEBUG)
		// When running with assertions enabled we track the owner.
		ASSERT(IsOwnedByCurrentThread());
		ThreadId saved_owner = owner_;
		owner_ = OSThread::kInvalidThreadId;
#endif // defined(DEBUG)

		Monitor::WaitResult retval = kNotified;
		if (micros == kNoTimeout)
		{
			// Wait forever.
			int result = pthread_cond_wait(data_.cond(), data_.mutex());
            ASSERT_PTHREAD_SUCCESS(result);
		}
		else
		{
			struct timespec ts;
			int64_t secs = micros / kMicrosPerSec;
			if (secs > kMaxInt32)
			{
				// Avoid truncation of overly large timeout values.
				secs = kMaxInt32;
			}
			int64_t nanos = (micros - (secs * kMicrosPerSec)) * kNanosPerMicro;
			ts.tv_sec = static_cast<int32_t>(secs);
			ts.tv_nsec = static_cast<long>(nanos); // NOLINT (long used in timespec).
			int result =
				pthread_cond_timedwait_relative_np(data_.cond(), data_.mutex(), &ts);
			ASSERT((result == 0) || (result == ETIMEDOUT));
			if (result == ETIMEDOUT)
			{
				retval = kTimedOut;
			}
		}

#if defined(DEBUG)
		// When running with assertions enabled we track the owner.
		ASSERT(owner_ == OSThread::kInvalidThreadId);
		owner_ = OSThread::GetCurrentThreadId();
		ASSERT(owner_ == saved_owner);
#endif // defined(DEBUG)
		return retval;
	}

	void Monitor::Notify()
	{
		// When running with assertions enabled we track the owner.
		ASSERT(IsOwnedByCurrentThread());
		int result = pthread_cond_signal(data_.cond());
        ASSERT_PTHREAD_SUCCESS(result);
	}

	void Monitor::NotifyAll()
	{
		// When running with assertions enabled we track the owner.
		ASSERT(IsOwnedByCurrentThread());
		int result = pthread_cond_broadcast(data_.cond());
        ASSERT_PTHREAD_SUCCESS(result);
	}

	bool Monitor::IsOwnedByCurrentThread() const
	{
#if defined(DEBUG)
		return owner_ == OSThread::GetCurrentThreadId();
#else
		UNREACHABLE();
		return false;
#endif
	}
} // namespace btrace
