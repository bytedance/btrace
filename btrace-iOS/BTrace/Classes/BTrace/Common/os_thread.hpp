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

#ifndef BTRACE_OS_THREAD_H_
#define BTRACE_OS_THREAD_H_

#include <string.h>
#include <pthread.h>
#include <mach/thread_info.h>
#include <pthread/introspection.h>

#ifdef __cplusplus

#include <mutex>
#include <atomic>
#include <random>
#include <cstring>
#include <algorithm>
#include <unordered_map>

#include "globals.hpp"
#include "allocation.hpp"
#include "zone.hpp"
#include "assert.hpp"

namespace btrace
{

#define VALIDATE_PTHREAD_RESULT(result)                         \
    if (result != 0)                                            \
    {                                                           \
        const int kBufferSize = 1024;                           \
        char error_message[kBufferSize];                        \
        Utils::StrError(result, error_message, kBufferSize);    \
        FATAL("pthread error: %d (%s)", result, error_message); \
    }

#if defined(PRODUCT)
#define VALIDATE_PTHREAD_RESULT_NAMED(result) VALIDATE_PTHREAD_RESULT(result)
#else
#define VALIDATE_PTHREAD_RESULT_NAMED(result)                               \
    if (result != 0)                                                        \
    {                                                                       \
        const int kBufferSize = 1024;                                       \
        char error_message[kBufferSize];                                    \
        Utils::StrError(result, error_message, kBufferSize);                \
        FATAL("pthread error: %d (%s)", result, error_message); \
    }
#endif

#if defined(DEBUG)
#define ASSERT_PTHREAD_SUCCESS(result) VALIDATE_PTHREAD_RESULT(result)
#else
    // NOTE: This (currently) expands to a no-op.
#define ASSERT_PTHREAD_SUCCESS(result) ASSERT(result == 0)
#endif

#ifdef DEBUG
#define RETURN_ON_PTHREAD_FAILURE(result)                                      \
    if (result != 0)                                                           \
    {                                                                          \
        const int kBufferSize = 1024;                                          \
        char error_message[kBufferSize];                                       \
        Utils::StrError(result, error_message, kBufferSize);                   \
        fprintf(stderr, "%s:%d: pthread error: %d (%s)\n", __FILE__, __LINE__, \
                result, error_message);                                        \
        return result;                                                         \
    }
#else
#define RETURN_ON_PTHREAD_FAILURE(result) \
    if (result != 0)                      \
        return result;
#endif

    // Forward declarations.
    class ThreadVisitor;

    typedef mach_port_t ThreadId;
    typedef pthread_t ThreadJoinId;
    typedef pthread_key_t ThreadLocalKey;

    static const ThreadLocalKey kUnsetThreadLocalKey =
        static_cast<pthread_key_t>(-1);

    // Low-level operations on OS platform threads.
    class OSThread
    {
    public:
        static constexpr intptr_t kNameBufferSize = 48;
        
        void SetRealTimePriority(uint32_t period = 3, double guaranteed = 0.5, double duty = 0.75);
        
        bool BasicInfo(thread_basic_info_data_t *info);

        bool InitThreadStack();

        ~OSThread();

        ThreadId id() const
        {
            //            ASSERT(id_ != OSThread::kInvalidThreadId);
            return id_;
        }
        
        ThreadJoinId joinId() const
        {
            // Warning! The "join_id" field is not very reliable.
            return join_id_;
        }
        
        static ThreadId MainThreadId()
        {
            return main_id_;
        }
        
        static ThreadJoinId MainJoinId()
        {
            return main_join_id_;
        }

        const char *name()
        {
            if (name_ == nullptr) {
                name_ = GetThreadName(join_id_);
            }
            return name_;
        }
        
        void set_name(const char *name);
                
        bool apple_thread();

        bool alive() const
        {
            return alive_;
        }
        
        bool dumped() const
        {
            return dumped_;
        }

        void set_dumped(bool dumped)
        {
            dumped_ = dumped;
        }

        float cpu_usage() const { return cpu_usage_; }
        void set_cpu_usage(float cpu_usage) {
            cpu_usage_ = cpu_usage;
        }
        
        uint64_t update_cpu_time();
        uint64_t cpu_time() const { return cpu_time_; }
        void set_cpu_time(uint64_t cpu_time) {
            cpu_time_ = cpu_time;
        }
        
        int64_t access_time() const { return access_time_; }
        inline void set_access_time(int64_t mach_time) {
            access_time_ = mach_time;
        }
        
        int64_t alloc_size() const { return alloc_size_; }
        void update_alloc_size(int64_t alloc_size) {
            alloc_size_ += alloc_size;
        }
        void set_alloc_size(int64_t alloc_size) {
            alloc_size_ = alloc_size;
        }
        
        int64_t alloc_count() const { return alloc_count_; }
        void update_alloc_count(int64_t alloc_count) {
            alloc_count_ += alloc_count;
        }
        void set_alloc_count(int64_t alloc_count) {
            alloc_count_ = alloc_count;
        }
        
        int64_t free_size() const { return free_size_; }
        void update_free_size(int64_t free_size) {
            free_size_ += free_size;
        }
        void set_free_size(int64_t free_size) {
            free_size_ = free_size;
        }
        
        int64_t free_count() const { return free_count_; }
        void update_free_count(int64_t free_count) {
            free_count_ += free_count;
        }
        void set_free_count(int64_t free_count) {
            free_count_ = free_count;
        }
        
//        uint32_t stack(uword *stack, uint32_t max_size) const {
//            uint32_t size = std::min(max_size, stack_size_);
//            memcpy(stack, stack_, size * sizeof(uword));
//            return size;
//        }
//        void set_stack(uword *stack, uint32_t size) {
//            size = std::min(kMaxStackDepth, size);
//            memcpy(stack_, stack, size * sizeof(uword));
//            stack_size_ = size;
//        }
        
        bool has_sampled() const { return has_sampled_; }
        void set_has_sampled(bool val) { has_sampled_ = val; }
        
        uword stack_base() const { return stack_base_; }
        uword stack_limit() const { return stack_limit_; }

        // Used to temporarily disable or enable thread interrupts.
        void DisableInterrupts();
        void EnableInterrupts();
        bool InterruptsEnabled();

        static void VisitThreads(ThreadVisitor *visitor);

        static Zone *zone() { return zone_; }
        static void set_zone(Zone *zone) { zone_ = zone; }

        static bool Active(thread_basic_info_data_t &info);

        static void ConstructThreadList();
        static void ResetThreadListState();
        static void ConstructThreadMap();
        static void ConstructThreads();
        
        static uint64_t CPUTime(thread_basic_info_data_t &info) {
            time_value_t user_time = info.user_time;
            time_value_t system_time = info.system_time;
            return (user_time.seconds + system_time.seconds) * kMicrosPerSec +
                    user_time.microseconds+system_time.microseconds;
        }
        
        static bool GetStackBounds(ThreadJoinId id, uword *lower, uword *upper);
        
        // The currently executing thread, or nullptr if not yet initialized.
        static OSThread *MainThread()
        {
            return main_thread_;
        }

        // The currently executing thread. If there is no currently executing thread,
        // a new OSThread is created and returned.
        static OSThread *Current()
	    {
            auto os_thread = GetCurrentTLS();

            if (os_thread != nullptr) {
                return os_thread;
            }

            os_thread = FindCurrent();

            if (os_thread != nullptr) {
                return os_thread;
            }

            os_thread = CreateOSThread();
            ASSERT(os_thread != nullptr);
            return os_thread;
        }

        static BTRACE_FORCE_INLINE void SetCurrent(OSThread *current)
        {
            SetCurrentTLS(current);
        }
        
        static OSThread *TryCurrent(bool is_main);

        static BTRACE_FORCE_INLINE OSThread *GetCurrentTLS()
        {
            return reinterpret_cast<OSThread *>(GetThreadLocal(thread_key_));
        }
        static BTRACE_FORCE_INLINE void SetCurrentTLS(OSThread *value)
        {
            // Provides thread-local destructors.
            SetThreadLocal(thread_key_, reinterpret_cast<uword>(value));
        }

        typedef void (*ThreadStartFunction)(uword parameter);
        typedef void (*ThreadDestructor)(void *parameter);

        // Start a thread running the specified function. Returns 0 if the
        // thread started successfully and a system specific error code if
        // the thread failed to start.
        static int New(const char *name,
                         ThreadStartFunction function,
                         uword parameter);

        static ThreadLocalKey CreateThreadLocal(
            ThreadDestructor destructor = nullptr);
        static void DeleteThreadLocal(ThreadLocalKey key);
        static ThreadId GetCurrentThreadId();
        static uint64_t GetThreadUid(ThreadId id);
        
        static BTRACE_FORCE_INLINE uword GetThreadLocal(ThreadLocalKey key)
        {
            ASSERT(key != kUnsetThreadLocalKey);
            if (__builtin_available(iOS 15, *))
            {
                return (uword)GetTsdDirect(key);
            }
            else
            {
                return (uword)pthread_getspecific(key);
            }
        }
        
        static BTRACE_FORCE_INLINE void SetThreadLocal(ThreadLocalKey key, uword value)
        {
            ASSERT(key != kUnsetThreadLocalKey);

            if (__builtin_available(iOS 15, *))
            {
                SetTsdDirect(key, reinterpret_cast<void *>(value));
            }
            else
            {
                int result = pthread_setspecific(key, reinterpret_cast<void *>(value));
                ASSERT_PTHREAD_SUCCESS(result);
            }
        }
        
        static BTRACE_FORCE_INLINE ThreadJoinId PthreadSelf()
        {
            if (__builtin_available(iOS 15, *))
            {
                return (ThreadJoinId)(TsdBase()[0]);
            }
            else
            {
                return pthread_self();
            }
        }
        
        static BTRACE_FORCE_INLINE void** TsdBase(void)
        {
            uint64_t tsd;
            __asm__ volatile ("mrs %0, TPIDRRO_EL0" : "=r" (tsd));

            return (void**)(uintptr_t)tsd;
        }

        static BTRACE_FORCE_INLINE void* GetTsdDirect(ThreadLocalKey slot)
        {
            return TsdBase()[slot];
        }

        static BTRACE_FORCE_INLINE void SetTsdDirect(ThreadLocalKey slot, const void *value)
        {
            TsdBase()[slot] = (void *)value;
        }
        
        static intptr_t GetMaxStackSize();
        static void Join(ThreadJoinId id);
        static void Detach(ThreadJoinId id);
        static bool Compare(ThreadId a, ThreadId b);

        static void Init();
        static void Start();

        static OSThread *FindCurrent();
        static OSThread *GetThreadById(ThreadId id);

        static void Stop();

        static void RemoveThreadFromList(OSThread *thread);
        static void RemoveThreadFromList(ThreadId id);
        static void RemoveThreadFromMap(ThreadId id);
        static void Lock() { thread_lock_->lock(); }
        static void Unlock() { thread_lock_->unlock(); }
        
        static BTRACE_FORCE_INLINE bool ThreadLogging(bool main=false) {
            if (main)
            {
                return main_logging_;
            }
            return GetThreadLocal(thread_logging_);
        }
        
        static BTRACE_FORCE_INLINE void DisableLogging(bool is_main) {
            if (is_main)
            {
                main_logging_ = true;
            }
            else
            {
                SetThreadLocal(thread_logging_, 1);
            }
        };
        
        static BTRACE_FORCE_INLINE void EnableLogging(bool is_main) {
            if (is_main)
            {
                main_logging_ = false;
            }
            else
            {
                SetThreadLocal(thread_logging_, 0);
            }
        };
        
        struct ScopedDisableLogging {
            ScopedDisableLogging(bool is_main):is_main_(is_main)
            {
                DisableLogging(is_main_);
            }
            
            ~ScopedDisableLogging()
            {
                EnableLogging(is_main_);
            }
            
            bool is_main_;
        };
        
        struct ScopedDisableInterrupt {
            ScopedDisableInterrupt(OSThread *os_thread): os_thread_(os_thread) {
                os_thread_->DisableInterrupts();
            }
            
            ~ScopedDisableInterrupt() {
                os_thread_->EnableInterrupts();
            }
            
        private:
            OSThread * os_thread_;
        };

        static constexpr ThreadId kInvalidThreadId = 0;
        static constexpr ThreadJoinId kInvalidThreadJoinId =
            static_cast<ThreadJoinId>(nullptr);

        static inline bool enable_thread_map_ = false;
        static inline bool enable_pthread_hook_ = false;
    private:
        // The constructor is private as CreateOSThread should be used
        // to create a new OSThread structure.
        OSThread(ThreadId id);
        OSThread();

        static OSThread *CreateOSThread(ThreadId tid=0);
        
        static void InitMainThread();

        static char *GetThreadName(ThreadJoinId id);

        static void AddThreadToList(OSThread *thread);
        static void AddThreadToMap(OSThread *thread);
        
        static void InstallPthreadHook();
        static void PthreadIntrospectionHook(unsigned int event, pthread_t thread, void *addr, size_t size);
        
        static inline ThreadId main_id_ = 0;
        static inline ThreadJoinId main_join_id_ = 0;

        ThreadId id_;
        ThreadJoinId join_id_ = kInvalidThreadJoinId;

        char *name_ = nullptr; // A name for this thread.
        bool alive_ = true;
        bool dumped_ = false;
        bool has_sampled_ = false;
        float cpu_usage_ = 0.1;
        uint64_t cpu_time_ = 0;
        int64_t access_time_ = 0;
        int64_t alloc_size_ = 0;
        int64_t alloc_count_ = 0;
        int64_t free_size_ = 0;
        int64_t free_count_ = 0;
        int32_t apple_thread_ = -1;

        // All |Thread|s are registered in the thread list.
        OSThread *thread_list_next_ = nullptr;

        // Thread interrupts disabled by default.
        std::atomic<uintptr_t> thread_interrupt_disabled_ = {0};

        uword stack_base_ = 0;
        uword stack_limit_ = 0;
//        uint32_t stack_size_ = 0;
//        uword stack_[kMaxStackDepth];
        
        static inline OSThread *main_thread_ = nullptr;
        static inline OSThread *thread_list_head_ = nullptr;
        static inline std::recursive_mutex *thread_lock_ = nullptr;
        static inline std::unordered_map<ThreadId, OSThread *>* thread_map_ = nullptr;
        
        static inline bool started_ = false;
        static inline std::once_flag pthread_hook_flag_;
        static inline pthread_introspection_hook_t prev_pthread_hook_ = NULL;
        
        static inline ThreadLocalKey thread_key_ = kUnsetThreadLocalKey;
        static inline ThreadLocalKey thread_logging_ = kUnsetThreadLocalKey;
        
        static inline bool main_logging_ = false;

        static inline Zone *zone_ = nullptr;

        friend class OSThreadIterator;
        friend class ThreadInterrupter;
        friend class SampleBufferProcessor;
        friend class ThreadInterrupterMacOS;
        friend class ProfilerStackWalker;
    };

    class ThreadVisitor
    {
    public:
        ThreadVisitor() {}
        virtual ~ThreadVisitor() {}

        virtual void VisitThread(OSThread *thread) = 0;

    private:
        DISALLOW_COPY_AND_ASSIGN(ThreadVisitor);
    };

    class OSThreadIterator : public ValueObject
    {
    public:
        OSThreadIterator();
        ~OSThreadIterator();

        // Returns false when there are no more threads left.
        bool HasNext() const;

        // Returns the current thread and moves forward.
        OSThread *Next();

    private:
        OSThread *next_;
        std::unordered_map<ThreadId, OSThread *>::iterator it;
    };

    class ThreadCPUUsageVisitor : public ThreadVisitor
    {
    public:
        ThreadCPUUsageVisitor(): total_usage_(0) {};
        virtual ~ThreadCPUUsageVisitor() = default;
        
        void VisitThread(OSThread *os_thread)
        {
            thread_basic_info_data_t info;
            bool success = os_thread->BasicInfo(&info);
            if (!success) {
                return;
            }
            
            if (!(info.flags & TH_FLAGS_IDLE)) {
                total_usage_ += info.cpu_usage / (float)TH_USAGE_SCALE;
            }
        }
        
        float total_usage() {
            return total_usage_;
        }
        
    private:
        float total_usage_;
    };

} // namespace btrace

#endif

#endif // BTRACE_OS_THREAD_H_
