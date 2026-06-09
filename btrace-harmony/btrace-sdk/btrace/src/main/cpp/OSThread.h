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
// Created on 2026/3/31.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef BTRACE_HARMONY_OSTHREAD_H
#define BTRACE_HARMONY_OSTHREAD_H

#include <bits/alltypes.h>
#include <cassert>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>

#include <mutex>
#include <vector>
#include <string>
#include <atomic>
#include <condition_variable>

#include "util/globals.h"
#include "util/lock_utils.h"

namespace btrace {
class OSThread {
public:
    using ThreadStartFunc = void *(*)(void *);
    using ThreadDestructor = void (*)(void *parameter);
    
    struct ScopedWorking {
        ScopedWorking(OSThread *os_thread): os_thread_(os_thread) {
            os_thread_->working_ = true;
            os_thread_->logging_ = true;
        }

        ~ScopedWorking() {
            os_thread_->working_ = false;
            os_thread_->logging_ = false;
        }
    private:
        OSThread * os_thread_;
    };
    
    pid_t tid() const { return tid_; }
    pthread_t join_id() const { return join_id_; }
    bool alive() { return alive_; }
    bool set_alive(bool flag) { return alive_ = flag; }
    
    void set_signal_disabled(bool flag) {
		signal_disabled_.store(flag, std::memory_order_relaxed);
	}

	bool signal_disabled(){
		return signal_disabled_.load(std::memory_order_relaxed);
	}
    
    void disableLogging() { logging_ = true; }
	void enableLogging(){ logging_ = false; }
    
    bool IsMain() { return this == main_thread_; }

    int64_t access_time() const { return access_time_; }
    inline void set_access_time(int64_t mach_time) {
        access_time_ = mach_time;
    }
    
    uint64_t *stack_buffer_ptr() {
        assert(IsMain());
        return stack_buffer_.data();
    }
    
    size_t stack_buffer_size() {
        assert(IsMain());
        return stack_buffer_.size();
    }

    const std::string &name() {
        if (name_.size() == 0) {
            char name[kNameBufferSize] = {0};
            if (GetThreadName(tid_, name, sizeof(name))) {
                name_ = std::string(name);
            }
        }
    
        return name_;
    }
    
    void* operator new(std::size_t size);
    void operator delete(void* p) noexcept;
    
    static pid_t MainTid() { return main_tid_; }

    static pthread_t MainJoinId() { return main_join_id_; }
    
    static bool PthreadAlive(pthread_t join_id);

    static bool Start();
    static bool Stop();
    
    static pid_t Pid() { return pid_; }
    
    static OSThread *CreateOSThread();
    static OSThread *TryCreateOSThread();
    
    static OSThread *Current();
    static OSThread *NonWorkingCurrent();
    static OSThread *TryCurrent();
    static OSThread *TryNonWorkingCurrent();
    static OSThread *SigSafeCurrent();
    static OSThread *SigSafeNonWorkingCurrent();
    
    static OSThread *GetThreadById(pid_t id);

    static BTRACE_FORCE_INLINE OSThread *GetCurrentTLS() {
        return reinterpret_cast<OSThread *>(GetThreadLocal(thread_key_));
    }

    static BTRACE_FORCE_INLINE void SetCurrentTLS(OSThread *value) {
        // Provides thread-local destructors.
        SetThreadLocal(thread_key_, reinterpret_cast<uint64_t>(value));
    }
    
    static pthread_key_t CreateThreadLocal(ThreadDestructor destructor=nullptr);
    
    static void DeleteThreadLocal(pthread_key_t key);
    
    static BTRACE_FORCE_INLINE uint64_t GetThreadLocal(pthread_key_t key) {
        assert(key != kUnsetThreadLocalKey);
        return (uint64_t)pthread_getspecific(key);
    }
        
    static BTRACE_FORCE_INLINE void SetThreadLocal(pthread_key_t key, uint64_t value) {
        assert(key != kUnsetThreadLocalKey);
        int result = pthread_setspecific(key, reinterpret_cast<void *>(value));
//        ASSERT_PTHREAD_SUCCESS(result);
    }

private:
    static constexpr pid_t kInvalidTid = -1;
    static constexpr pthread_t kInvalidJoinId = -1;
    static constexpr int32_t kNameBufferSize = 64;
    static constexpr pthread_key_t kUnsetThreadLocalKey = -1;
    static constexpr int32_t kPthreadNextDefaultOffset = -1;
    static constexpr int64_t kInfoUpdatePeriodS = 1;
    
    OSThread(pid_t id, pthread_t join_id);
    
    static void InitMainThread();
    static void ConstructThreads();

    static void WaitForWorkingThread();
    static bool CheckWorkingThreadUnlock();
    
    static void AddThreadToList(OSThread *os_thread);
    static void RemoveThreadFromList(OSThread *os_thread);
    static void DeleteDeadThreadUnlock();
    
    static bool RegisterTimer(); 
    static void UpdateInfo(union sigval);
    
    using pthread_create_func = int (*)(pthread_t *__restrict, const pthread_attr_t *__restrict, ThreadStartFunc, void *__restrict);
    static void InstallPthreadHook(bool *res);
    static int PthreadCreateHook(pthread_t *__restrict, const pthread_attr_t *__restrict, ThreadStartFunc, void *__restrict);
    
    static void *ThreadStart(void *data_ptr);
    static void OnThreadExit(void *val);

    static bool GetThreadName(pthread_t id, char *buffer, size_t size);
    static bool GetThreadName(pid_t id, char *buffer, size_t size);
    static bool CalcPthreadNextOffset();

    pid_t tid_ = kInvalidTid;
    pthread_t join_id_ = kInvalidJoinId;
    int64_t access_time_ = 0;
    std::string name_;
    std::vector<uint64_t> stack_buffer_{};
    bool alive_ = false;
    // use a separate 'logging_' field to avoid the overhead of atomic variables.
    bool logging_ = false;
    std::atomic<bool> working_ = false;
    std::atomic<bool> signal_disabled_ = false;
    OSThread *next_ = nullptr;
    
    static inline bool started_ = false;
    static inline pid_t pid_ = 0;
    static inline OSThread *main_thread_ = nullptr;
    static inline pid_t main_tid_ = 0;
    static inline pthread_t main_join_id_ = 0;
    static inline OSThread *thread_list_head_ = nullptr;
//    static inline std::recursive_mutex *thread_lock_ = nullptr;
    static inline RecursiveSpinlock *thread_lock_ = nullptr;
    static inline pthread_key_t thread_key_ = kUnsetThreadLocalKey;
    
    static inline std::once_flag pthread_hook_flag_;
    static inline pthread_create_func pthread_create_origin_;
    static inline std::atomic<int32_t> pthread_next_offset_ = kPthreadNextDefaultOffset;

    static inline timer_t update_timer_ = nullptr;

    friend class OSThreadIterator;
};

class OSThreadIterator {
public:
    OSThreadIterator();
    ~OSThreadIterator();

    // Returns false when there are no more threads left.
    bool HasNext() const;

    // Returns the current thread and moves forward.
    OSThread *Next();

private:
    OSThread *os_thread_ = nullptr;
};
}


#endif //BTRACE_HARMONY_OSTHREAD_H
