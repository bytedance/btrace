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
// Created on 2026/4/2.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "lock_utils.h"
#include "util/globals.h"

#include <thread>

#include <time.h>
#include <syscall.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>

namespace btrace {
    void Spinlock::lock() {
        int attempt = 0;
        int64_t sleep_cnt = 0;
        
        // We need to start with attempt = 1, otherwise
        // attempt % kLockAttemptsPerSleep is zero for the first iteration.
        while (1) {
            attempt += 1;
            
            if (!locked_.load(std::memory_order_relaxed) &&
                !locked_.exchange(true, std::memory_order_acquire)) {
                break;
            }
            
            if ((attempt & 0x3ff) == 0) {
                sleep_cnt++;
            	struct timespec ts = {
		            .tv_sec = 0,
		            .tv_nsec = std::min(sleep_cnt * 10, (int64_t)1000) * kNanosPerMicro // sleep_cnt * 10us
	            };
                // Do not use nanosleep() directly as we have hooked it,
                // and direct calls will cause infinite recursion
                int err = errno;
                syscall(SYS_nanosleep, &ts, &ts);
                errno = err;
            } else {
                __asm__ __volatile__("isb" ::: "memory");
            }
        }
    }

    void RecursiveSpinlock::lock() {
        pthread_t join_id = pthread_self();

        if (owner_ != join_id) {
            lock_.lock();
            owner_.store(join_id, std::memory_order_release);
        }
        count_++;
    }
    
    bool RecursiveSpinlock::try_lock() noexcept {
        pthread_t join_id = pthread_self();
        
        if (lock_.try_lock()) {
            owner_.store(join_id, std::memory_order_release);
        } else {
            if (owner_.load(std::memory_order_acquire) != join_id) {
                return false;
            }
        }

        count_++;
        return true;
    }
    
    void RecursiveSpinlock::unlock() noexcept {
        if (--count_ == 0) {
            owner_.store(0, std::memory_order_release);
            lock_.unlock();
        }
    }
}