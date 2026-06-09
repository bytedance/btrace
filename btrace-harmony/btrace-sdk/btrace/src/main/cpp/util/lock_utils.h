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

#ifndef BTRACE_HARMONY_LOCK_UTILS_H
#define BTRACE_HARMONY_LOCK_UTILS_H

#include <atomic>

namespace btrace {

struct Spinlock {

        Spinlock() = default;

        Spinlock(const Spinlock&)            = delete;
        Spinlock& operator=(const Spinlock&) = delete;
        
        void lock();
        
        bool try_lock() noexcept {
            return !locked_.load(std::memory_order_relaxed) &&
            !locked_.exchange(true, std::memory_order_acquire);
        }
        
        void unlock() noexcept {
            locked_.store(false, std::memory_order_release);
        }
    private:
        std::atomic<uint8_t> locked_ = false;
    };

    struct RecursiveSpinlock {
        
        RecursiveSpinlock() = default;

        RecursiveSpinlock(const RecursiveSpinlock&)            = delete;
        RecursiveSpinlock& operator=(const RecursiveSpinlock&) = delete;
        
        void lock();
        
        bool try_lock() noexcept;
        
        void unlock() noexcept;
    private:
        Spinlock lock_;
        std::atomic<uint64_t> owner_ = 0;
        int32_t count_ = 0;
};

}

#endif //BTRACE_HARMONY_LOCK_UTILS_H
