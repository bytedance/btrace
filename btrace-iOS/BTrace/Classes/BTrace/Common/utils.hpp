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

#ifndef BTRACE_UTILS_H_
#define BTRACE_UTILS_H_

#include <mach/mach_time.h> // NOLINT
#include <sys/time.h> // NOLINT
#include <execinfo.h>
#include <mach/mach.h>
#include <mach/vm_types.h>

#ifdef __cplusplus
#include <limits>
#include <vector>
#include <memory>
#include <algorithm>
#include <type_traits>
#include <cassert>

#include "assert.hpp"
#include "globals.hpp"
#include "allocation.hpp"

#ifndef likely
#define likely(x) (__builtin_expect((int)(x), 1))
#endif

#ifndef unlikely
#define unlikely(x) (__builtin_expect((int)(x), 0))
#endif

#define INSTACK(a) ((a) >= stackbot && (a) <= stacktop)
#if defined(__arm__) || defined(__arm64__)
#define ISALIGNED(a) ((((uintptr_t)(a)) & 0x1) == 0)
#endif

#define IsValidPointer(x) ((uintptr_t)x >= PAGE_SIZE)

#if defined(__arm64__) || defined(__arm64e__)
#define CLS_PTRAUTH_STRIP(pointer) ((uintptr_t)pointer & 0x0000000FFFFFFFFF)
#define GET_IP_REGISTER(r) (CLS_PTRAUTH_STRIP(arm_thread_state64_get_pc(r->__ss)))
#define GET_FP_REGISTER(r) (CLS_PTRAUTH_STRIP(arm_thread_state64_get_fp(r->__ss)))
#define GET_SP_REGISTER(r) (CLS_PTRAUTH_STRIP(arm_thread_state64_get_sp(r->__ss)))
#define GET_LR_REGISTER(r) (CLS_PTRAUTH_STRIP(arm_thread_state64_get_lr(r->__ss)))
#define SET_IP_REGISTER(r, v) arm_thread_state64_set_pc_fptr(r->__ss, (void *)v)
#define SET_FP_REGISTER(r, v) arm_thread_state64_set_fp(r->__ss, v)
#define SET_SP_REGISTER(r, v) arm_thread_state64_set_sp(r->__ss, v)
#define SET_LR_REGISTER(r, v) arm_thread_state64_set_lr_fptr(r->__ss, (void *)v)
#endif

#if defined(__arm__) || defined(__arm64__)
#define ISALIGNED(a) ((((uintptr_t)(a)) & 0x1) == 0)
#endif

namespace btrace
{

    using Stack = std::vector<uword>;
    using SharedStack = std::shared_ptr<Stack>;

    template<typename T>
    struct get_type {
        using type = T;
    };

    template<typename T>
    struct get_type<T*> {
        using type = T;
    };

    struct PthreadStackRange {
        uword *stack_bot;
        uword *stack_top;
        
        PthreadStackRange(uword *stack_bot, uword *stack_top): stack_bot(stack_bot), stack_top(stack_top) {};
    };

    template<typename T>
    struct ScopedCounter {
        ScopedCounter(std::atomic<T> &count, std::atomic<bool> &main_running, bool is_main):
            count_(count), main_running_(main_running), is_main_(is_main) {
                if (is_main_) {
                    main_running_.store(true, std::memory_order_relaxed);
                }
                else {
                    count_.fetch_add(1, std::memory_order_relaxed);
                }
            }
        
        ~ScopedCounter() {
            if (is_main_) {
                main_running_.store(false, std::memory_order_relaxed);
            }
            else {
                count_.fetch_sub(1, std::memory_order_relaxed);
            }
        }
        
    private:
        std::atomic<T> &count_;
        std::atomic<bool> &main_running_;
        bool is_main_;
    };

    class Utils
    {
    public:
        static BTRACE_FORCE_INLINE void HashCombine(uint64_t& h, uint64_t k)
        {
            const uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
            const int r = 47;

            k *= m;
            k ^= k >> r;
            k *= m;

            h ^= k;
            h *= m;

            // Completely arbitrary number, to prevent 0's
            // from hashing to 0.
            h += 0xe6546b64;
        }

        static uint64_t MemoryUsage() {
            task_vm_info_data_t vmInfo;
            mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
            kern_return_t kernelReturn = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t) &vmInfo, &count);
            if(kernelReturn == KERN_SUCCESS) {
                return vmInfo.phys_footprint;
            } else {
                return 0;
            }
        }
        
        static BTRACE_FORCE_INLINE bool HitSampling(uint64_t num, uint64_t rate) {
            return num % rate == 0;
        }
        
        __attribute__((noinline, not_tail_called)) static int
        ThreadStackPCs(uword *buffer, unsigned max, unsigned skip,
                       PthreadStackRange *stack_range=nullptr);

        static SharedStack GetThreadStack(int skip, PthreadStackRange *stack_range=nullptr)
        {
            skip += 1;
            uword buffer[kMaxStackDepth];
            constexpr int max_size = sizeof(buffer) / sizeof(uword);
            int stack_size = ThreadStackPCs(buffer, max_size, skip, stack_range);
            auto buffer_ptr = (uword*)buffer;
            auto result = std::make_shared<Stack>(buffer_ptr, buffer_ptr + stack_size);

            return result;
        }
        
        template <typename T>
        static constexpr bool IsAligned(T x,
                                        uintptr_t alignment,
                                        uintptr_t offset = 0)
        {
            ASSERT(offset < alignment);
            return (x & (alignment - 1)) == offset;
        }

        template <typename T>
        static constexpr bool IsAligned(T *x,
                                        uintptr_t alignment,
                                        uintptr_t offset = 0)
        {
            return IsAligned(reinterpret_cast<uword>(x), alignment, offset);
        }

        template <typename T>
        static constexpr inline T RoundDown(T x, intptr_t alignment)
        {
            return (x & -alignment);
        }

        template <typename T>
        static inline T *RoundDown(T *x, intptr_t alignment)
        {
            return reinterpret_cast<T *>(
                RoundDown(reinterpret_cast<uword>(x), alignment));
        }

        template <typename T>
        static constexpr inline T RoundUp(T x,
                                          uintptr_t alignment)
        {
//            ASSERT(offset < alignment);
            return RoundDown(x + alignment - 1, alignment) ;
        }

        template <typename T>
        static inline T *RoundUp(T *x, uintptr_t alignment)
        {
            return reinterpret_cast<T *>(
                RoundUp(reinterpret_cast<uword>(x), alignment));
        }

        // Implementation is from "Hacker's Delight" by Henry S. Warren, Jr.,
        // figure 3-3, page 48, where the function is called clp2.
        static constexpr uintptr_t RoundUpToPowerOfTwo(uintptr_t x)
        {
            x = x - 1;
            x = x | (x >> 1);
            x = x | (x >> 2);
            x = x | (x >> 4);
            x = x | (x >> 8);
            x = x | (x >> 16);
#if defined(ARCH_IS_64_BIT)
            x = x | (x >> 32);
#endif // defined(ARCH_IS_64_BIT)
            return x + 1;
        }

        static inline char *StrError(int err, char *buffer, size_t bufsize)
        {
            if (strerror_r(err, buffer, bufsize) != 0)
            {
                snprintf(buffer, bufsize, "%s", "strerror_r failed");
            }
            return buffer;
        }

        // Returns the current process id.
        static intptr_t ProcessId();

        static void MachTimeInit();

        // wait for a while, unit: ms.
        static void WaitFor(uint64_t wait_time);
        
        // mach ticks
        static void WaitUntil(uint64_t wait_ticks);

        static uint64_t MillsToMachTicks(uint64_t time_delta);
        
        static uint64_t MicrosToMachTicks(uint64_t time_delta);
        
        static BTRACE_FORCE_INLINE uint64_t MachTicksNow()
        {
            return mach_approximate_time();
        }

        static BTRACE_FORCE_INLINE uint64_t GetCurrMachNanos()
        {
            ASSERT(timebase_info_.denom != 0);
            // timebase_info converts continuous time tick units into nanoseconds.
            uint64_t result = MachTicksNow();
            result *= timebase_info_.numer;
            result /= timebase_info_.denom;
            return result;
        }

        static BTRACE_FORCE_INLINE uint64_t GetCurrMachMicros()
        {
           return GetCurrMachNanos() / kNanosPerMicro;
        }

        static BTRACE_FORCE_INLINE uint64_t GetCurrMachMillis()
        {
           return GetCurrMachNanos() / kNanosPerMilli;
        }

        // Returns the current time in microseconds measured
        // from midnight January 1, 1970 UTC.
        static int64_t GetCurrentTimeMicros();
        
        static int64_t GetCurrentTimeMillis();
        
        static uint64_t GetClockMonotonicMicros();

        // Print formatted output to stdout/stderr for debugging.
        // Tracing and debugging prints from the VM should strongly prefer to use
        // PrintErr to avoid interfering with the application's output, which may
        // be parsed by another program.
        static void Print(const char *format, ...) PRINTF_ATTRIBUTE(1, 2);
        static void PrintErr(const char *format, ...) PRINTF_ATTRIBUTE(1, 2);
        static void VFPrint(FILE *stream, const char *format, va_list args);
    private:
        static inline int64_t init_time_interval_; // us
        static inline int64_t init_mach_micros_; // us
        static inline mach_timebase_info_data_t timebase_info_;
    };

} // namespace btrace

#endif
#endif // BTRACE_UTILS_H_
