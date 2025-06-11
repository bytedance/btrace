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

// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "utils.hpp"
#include "os_thread.hpp"
#include "globals.hpp"

#if BTRACE_HOST_OS_IOS
#include <syslog.h> // NOLINT
#endif

namespace btrace
{

    __attribute__((noinline, not_tail_called)) int
    Utils::ThreadStackPCs(uword *buffer, unsigned max, unsigned skip,
                          PthreadStackRange *stack_range)
    {
        int nb = 0;
        uword *frame, *next;
        uword *stacktop, *stackbot;
        
        if (stack_range == nullptr) {
            pthread_t self = OSThread::PthreadSelf();
            stacktop = (uword *)pthread_get_stackaddr_np(self);
            stackbot = (uword *)((uint64_t)stacktop - pthread_get_stacksize_np(self));
        } else {
            stacktop = stack_range->stack_top;
            stackbot = stack_range->stack_bot;
        }

        frame = (uword *)__builtin_frame_address(0);
        next = * (uword **)frame;
        
        if(!INSTACK(frame) || !ISALIGNED(frame))
            return nb;
        
        while (skip--) {
            if(!INSTACK(next) || !ISALIGNED(next) || next <= frame)
                return nb;
            
            frame = next;
            next = *(uword **)frame;
        }
        
        while (max--) {
            uword retaddr = CLS_PTRAUTH_STRIP(*(uword *)(frame + 1));
            
            if (retaddr == 0)
                return nb;

            next = *(uword **)frame;
            buffer[nb] = retaddr;
            nb++;
            
            if(!INSTACK(next) || !ISALIGNED(next) || next <= frame)
                return nb;
            
            frame = next;
        }
        
        return nb;
    }

	intptr_t Utils::ProcessId()
	{
		return static_cast<intptr_t>(getpid());
	}

	int64_t Utils::GetCurrentTimeMicros()
	{
        int64_t timestamp = init_time_interval_;
        timestamp += GetCurrMachMicros() - init_mach_micros_;
        return timestamp;
	}

    int64_t Utils::GetCurrentTimeMillis()
    {
        return GetCurrentTimeMicros() / kMicrosPerMilli;
    }
    
    void Utils::MachTimeInit()
    {
        mach_timebase_info(&timebase_info_);
        
        // gettimeofday has microsecond resolution.
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        init_time_interval_ = static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
        init_mach_micros_ = GetCurrMachMicros();
    }

	void Utils::WaitFor(uint64_t wait_time)
	{
		uint64_t time_to_wait = wait_time * NANOS_PER_MILLISEC * timebase_info_.denom / timebase_info_.numer;
		uint64_t now = mach_approximate_time();
		mach_wait_until(now + time_to_wait);
	}

    void Utils::WaitUntil(uint64_t wait_time)
    {
        mach_wait_until(wait_time);
    }

    uint64_t Utils::MillsToMachTicks(uint64_t time_delta)
    {
        return time_delta * NANOS_PER_MILLISEC * timebase_info_.denom / timebase_info_.numer;
    }

    uint64_t Utils::MicrosToMachTicks(uint64_t time_delta)
    {
        return time_delta * NANOS_PER_USEC * timebase_info_.denom / timebase_info_.numer;
    }

    uint64_t Utils::GetClockMonotonicMicros()
    {
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        return tp.tv_sec * kMicrosPerSec + tp.tv_nsec / kNanosPerMicro;
    }

	void Utils::Print(const char *format, ...)
	{
		char *prefix_format;
		asprintf(&prefix_format, "[BTrace] %s", format);
#if BTRACE_HOST_OS_IOS
		va_list args;
		va_start(args, format);
		vsyslog(LOG_NOTICE, prefix_format, args);
		va_end(args);
#else
		va_list args;
		va_start(args, format);
		VFPrint(stdout, prefix_format, args);
		va_end(args);
#endif
		free(prefix_format);
	}

	void Utils::VFPrint(FILE *stream, const char *format, va_list args)
	{
		vfprintf(stream, format, args);
		fflush(stream);
	}

	void Utils::PrintErr(const char *format, ...)
	{
		char *prefix_format;
		asprintf(&prefix_format, "[BTrace] [Error] %s", format);
#if BTRACE_HOST_OS_IOS
		va_list args;
		va_start(args, format);
		vsyslog(LOG_ERR, prefix_format, args);
		va_end(args);
#else
		va_list args;
		va_start(args, format);
		VFPrint(stderr, prefix_format, args);
		va_end(args);
#endif
		free(prefix_format);
	}

} // namespace btrace
