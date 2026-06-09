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

#include "OSThread.h"
#include "util/memory_utils.h"

#include <bits/alltypes.h>
#include <chrono>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <hilog/log.h>

#include "util/hook_utils.h"

#include <vector>
#include <thread>
#include <filesystem>

#undef LOG_TAG
#define LOG_TAG "btrace::OSThread"

namespace btrace {
static int ReadThreadNameInfo(pid_t tid, char *buffer, size_t size) {
    int fd, status = 0;
	char f[sizeof "/proc/self/task//comm" + 3*sizeof(int)];

	if (size < 16) return ERANGE;

	if (tid == gettid())
		return prctl(PR_GET_NAME, (unsigned long)buffer, 0UL, 0UL, 0UL) ? errno : 0;

	snprintf(f, sizeof f, "/proc/self/task/%d/comm", tid);

	if ((fd = open(f, O_RDONLY|O_CLOEXEC)) < 0 || (size = read(fd, buffer, size)) == -1) status = errno;
	else buffer[size-1] = 0; /* remove trailing new line only if successful */
	if (fd >= 0) close(fd);

	return status;
}

class ThreadStartData {
public:
    ThreadStartData(OSThread::ThreadStartFunc function, void *arg)
			: function_(function), arg_(arg) {}
    
    OSThread::ThreadStartFunc function() const { return function_; }
    
    void *arg() const { return arg_; }

	private:
		OSThread::ThreadStartFunc function_;
		void *arg_;

		DISALLOW_COPY_AND_ASSIGN(ThreadStartData);
};

void *OSThread::ThreadStart(void *data_ptr) {
    ThreadStartData *data = reinterpret_cast<ThreadStartData *>(data_ptr);
    
    ThreadStartFunc function = data->function();
    void *arg = data->arg();
    delete data;

    OSThread *thread = CreateOSThread();
    
    void *res = function(arg);

    return res;
}

void OSThread::OnThreadExit(void *val) {
    OSThread *os_thread = reinterpret_cast<OSThread *>(val);
    if (os_thread == nullptr) { return; }
    os_thread->alive_ = false;
}

bool OSThread::Start() {
    if (thread_lock_ == nullptr) {
        // do not delete thread_lock_
//        thread_lock_ = new std::recursive_mutex();
        thread_lock_ = new RecursiveSpinlock();
    }
    
    {
        std::lock_guard ml(*thread_lock_);
        
        if (started_) { return true; }
        
        // Create the thread local key.
        if (thread_key_ == kUnsetThreadLocalKey) {
            thread_key_ = CreateThreadLocal(OnThreadExit);
        }
        assert(thread_key_ != kUnsetThreadLocalKey);
        started_ = true;
    }
    
    bool res = true;
	std::call_once(pthread_hook_flag_, InstallPthreadHook, &res);
//	EventLoop::Register(main_update_period_, UpdateCPUUsage);
    
    if (!res) {
        return false;
    }
    
    if (pid_ == 0) {
        pid_ = getpid();
    }
    
    ConstructThreads();
    InitMainThread();
    
    if (main_thread_ == nullptr) {
        return false;
    }
    
    RegisterTimer();
    
    return true;
}

void OSThread::InitMainThread() {
    OSThread *thread = nullptr;
    
    OSThreadIterator it;
    while (it.HasNext()) {
        OSThread *t = it.Next();
        if (t->tid_ == pid_) {
            thread = t;
            break;
        }
    }
    
    assert(thread != nullptr);
    
    if (thread != nullptr) {
        thread->stack_buffer_.resize(kMaxMainStackDepth);
        main_thread_ = thread;
        main_tid_ = thread->tid_;
        main_join_id_ = thread->join_id_;
    }
}

void OSThread::ConstructThreads() {
    int offset = pthread_next_offset_.load();
    
    if (offset == kPthreadNextDefaultOffset) {
        return;
    }
    
    pthread_t self = pthread_self();
    
    std::lock_guard ml(*thread_lock_);
    
    if (!started_) { return; }
    
    int cnt = 0;
    pthread_t t = self;
    
    do {
        cnt++;
        pid_t tid = pthread_gettid_np(t);
        
        if (tid == kInvalidTid) { break; }
        
        OSThread *os_thread = OSThread::GetThreadById(tid);

        if (!os_thread) {
            os_thread = new OSThread(tid, t);
            AddThreadToList(os_thread);
        } else {
            os_thread->alive_ = true;
        }
        
        pthread_t next = kInvalidJoinId;
        bool res = SafeReadMemory((uint64_t*)t+offset, &next, sizeof(next));
        
        if (!res) { break; }
        
        t = next;
    } while (t != self && cnt <= 1024);
}

bool OSThread::Stop() {
    std::lock_guard ml(*thread_lock_);
    
    if (!started_) { return true; }
    
    if (update_timer_ != nullptr) {
        timer_delete(update_timer_);
        update_timer_ = nullptr;
    }
    
    WaitForWorkingThread();
    DeleteDeadThreadUnlock();
    
    started_ = false;
    
    return true;
}

pthread_key_t OSThread::CreateThreadLocal(ThreadDestructor destructor) {
    pthread_key_t key = kUnsetThreadLocalKey;
	int result = pthread_key_create(&key, destructor);
    // ASSERT_PTHREAD_SUCCESS(result);
	assert(key != kUnsetThreadLocalKey);
	return key;
}

void OSThread::DeleteThreadLocal(pthread_key_t key) {
    assert(key != kUnsetThreadLocalKey);
    int result = pthread_key_delete(key);
    // ASSERT_PTHREAD_SUCCESS(result);
}

OSThread::OSThread(pid_t tid, pthread_t join_id) {
    tid_ = tid;
    join_id_ = join_id;
    alive_ = true;
}

void* OSThread::operator new(std::size_t size) {
    void *p = malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void OSThread::operator delete(void* p) noexcept {
    free(p);
}

OSThread *OSThread::CreateOSThread() {
    if (!started_) { return nullptr; }
    
    pthread_t join_id = pthread_self();
    pid_t tid = gettid();
    
    std::lock_guard ml(*thread_lock_);

    if (!started_) { return nullptr; }

    auto os_thread = GetThreadById(tid);

    if (os_thread && os_thread->join_id_ == join_id) {
        OSThread::SetCurrentTLS(os_thread);
        return os_thread;
    } else if (os_thread) {
        os_thread->alive_ = false;
    }

    os_thread = new OSThread(tid, join_id);

    if (os_thread == nullptr) { return os_thread; }
    
    assert(os_thread->alive_ = true);
    OSThread::SetCurrentTLS(os_thread);
    AddThreadToList(os_thread);
    
    return os_thread;
}

OSThread *OSThread::TryCreateOSThread() {
    if (!started_) { return nullptr; }
    
    pthread_t join_id = pthread_self();
    pid_t tid = gettid(); // pthread_gettid_np(join_id) can cause a deadlock
    
    OSThread *os_thread = nullptr;
    
    bool res = thread_lock_->try_lock();
    if (!res) { return nullptr; }

    if (!started_) { goto end; }

    os_thread = GetThreadById(tid);

    if (os_thread && os_thread->join_id_ == join_id) {
        OSThread::SetCurrentTLS(os_thread);
        goto end;
    } else if (os_thread) {
        os_thread->alive_ = false;
    }

    os_thread = new OSThread(tid, join_id);

    if (os_thread == nullptr) { goto end; }
    
    assert(os_thread->alive_ = true);
    OSThread::SetCurrentTLS(os_thread);
    AddThreadToList(os_thread);
    
end:
    thread_lock_->unlock();
    return os_thread;
}

void OSThread::DeleteDeadThreadUnlock() {
	OSThread *current = thread_list_head_;
	OSThread *next = nullptr;
	OSThread *previous = nullptr;

	// Scan across list and remove dead(not alive) thread.
	while (current != nullptr) {
		next = current->next_;

		if (!current->alive_) {
			if (previous == nullptr) {
				thread_list_head_ = next;
			} else {
				previous->next_ = next;
			}
			delete current;
		} else {
			previous = current;
		}

		current = next;
	}
}

OSThread *OSThread::GetThreadById(pid_t id) {
    if (id == OSThread::kInvalidTid) {
        return nullptr;
    }

    OSThread *current = thread_list_head_;
    OSThread *next = nullptr;

    while (current != nullptr) {
        if (current->alive_ && current->tid_ == id) {
            return current;
        }
        next = current->next_;
        current = next;
    }

    return nullptr;
}

void OSThread::AddThreadToList(OSThread *os_thread) {
    assert(os_thread != nullptr);
    assert(os_thread->next_ == nullptr);

    // Insert at head of list.
    os_thread->next_ = thread_list_head_;
    thread_list_head_ = os_thread;
}

void OSThread::RemoveThreadFromList(OSThread *os_thread) {
    if (!started_) { return; }
    
	std::lock_guard ml(*thread_lock_);
    
    if (!started_) { return; }
    
	OSThread *current = thread_list_head_;
	OSThread *previous = nullptr;

	// Scan across list and remove |thread|.
	while (current != nullptr) {
		if (current == os_thread) {
			// We found |thread|, remove from list.
			if (previous == nullptr) {
				thread_list_head_ = current->next_;
			} else {
				previous->next_ = current->next_;
			}

			current->next_ = nullptr;
            delete current;
			break;
		}

		previous = current;
		current = current->next_;
	}
}

OSThread *OSThread::Current() {
    auto os_thread = GetCurrentTLS();

    if (os_thread == nullptr) {
        // use TryCreateOSThread() to avoid dead lock
        os_thread = TryCreateOSThread();
    }

    return os_thread;
}

OSThread *OSThread::NonWorkingCurrent() {
    auto os_thread = Current();

	if (os_thread != nullptr && os_thread->logging_) {
		return nullptr;
	}

	return os_thread;
}

OSThread *OSThread::TryCurrent() {
    auto os_thread = GetCurrentTLS();

    if (os_thread != nullptr) { return os_thread; }

    auto join_id = pthread_self();
    auto tid = gettid(); // pthread_gettid_np(join_id) can cause deadlock

    bool res = thread_lock_->try_lock();
    if (!res) { return nullptr; }

    if (!started_) { goto end; }

    os_thread = GetThreadById(tid);

    if (os_thread && os_thread->join_id_ == join_id) {
        OSThread::SetCurrentTLS(os_thread);
    } else if (os_thread) {
        os_thread->alive_ = false;
    } else {
        os_thread = nullptr;
    }

end:
    thread_lock_->unlock();
    return os_thread;
}

OSThread *OSThread::TryNonWorkingCurrent() {
    auto os_thread = TryCurrent();

	if (os_thread != nullptr && os_thread->logging_) {
		return nullptr;
	}

	return os_thread;
}

OSThread *OSThread::SigSafeCurrent() {
    auto os_thread = GetCurrentTLS();

    if (os_thread != nullptr) { return os_thread; }

    auto join_id = pthread_self();
    auto tid = gettid(); // pthread_gettid_np(join_id) can cause deadlock

    bool res = thread_lock_->try_lock();
    if (!res) { return nullptr; }

    if (!started_) { goto end; }

    os_thread = GetThreadById(tid);

    if (os_thread && os_thread->join_id_ == join_id) {
        OSThread::SetCurrentTLS(os_thread);
    } else if (os_thread) {
        os_thread->alive_ = false;
    } else {
        os_thread = nullptr;
    }

end:
    thread_lock_->unlock();
    return os_thread;
}

OSThread *OSThread::SigSafeNonWorkingCurrent() {
    auto os_thread = SigSafeCurrent();

	if (os_thread != nullptr && os_thread->logging_) {
		return nullptr;
	}

	return os_thread;
}

void OSThread::InstallPthreadHook(bool *res) {
    hook_init();
    hook_all("pthread_create", (void *)PthreadCreateHook, (void **)&pthread_create_origin_);
    *res = CalcPthreadNextOffset();
}

int OSThread::PthreadCreateHook(pthread_t *__restrict t, const pthread_attr_t *__restrict attr, void *(*entry)(void *),
                                void *__restrict arg) {
    ThreadStartData *data = new ThreadStartData(entry, arg);
    return pthread_create_origin_(t, attr, ThreadStart, data);
}

bool OSThread::GetThreadName(pthread_t id, char *buffer, size_t size) {
	return pthread_getname_np(id, buffer, size) == 0;
}

bool OSThread::GetThreadName(pid_t tid, char *buffer, size_t size){
    return ReadThreadNameInfo(tid, buffer, size) == 0;
}

bool OSThread::CalcPthreadNextOffset() {
    if (pthread_next_offset_.load() != kPthreadNextDefaultOffset) {
        return true;
    }
    
    pthread_t self = pthread_self();
    pthread_t child = 0;
    bool exit = false;
    
    std::mutex mtx;
    std::condition_variable cv;
    
    auto t = std::thread([&child, &exit, &mtx, &cv](){
        // Note that the 'next' field in struct pthread is not updated atomically, 
        // so it is possible that in the child thread the correct 'offset' cannot be found.
        {
            std::unique_lock<std::mutex> lock(mtx);
            child = pthread_self();
        }
    
        cv.notify_one();
        
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&exit] { return exit; });
    });
    
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&child] { return child != 0; });
        
        int offset = kPthreadNextDefaultOffset;
        for (int i = 0; i <= 8; i+=1) { // 8个指针大小范围内遍历
            pthread_t pthread_next = 0;
            bool res = SafeReadMemory((uint64_t*)self+i, &pthread_next, sizeof(pthread_next));
            
            if (!res) { break; }

            if (pthread_next == child) {
                offset = i;
                break;
            }
        }

        if (offset != kPthreadNextDefaultOffset) {
            pthread_next_offset_ = offset;
        }
        
        exit = true;
    }
    
    cv.notify_one();
    t.join();
    
    bool res = pthread_next_offset_.load() != kPthreadNextDefaultOffset;
    
    if (!res) {
        return false;
    }
    
    return true;
}

bool OSThread::PthreadAlive(pthread_t join_id) {
    return pthread_gettid_np(join_id) != -1;
}

void OSThread::WaitForWorkingThread() {
	while (CheckWorkingThreadUnlock()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

bool OSThread::CheckWorkingThreadUnlock() {
	bool res = false;

	OSThread *current = thread_list_head_;
	OSThread *next = nullptr;

	// Scan across list.
	while (current != nullptr) {
		res = res || current->working_;
		next = current->next_;
		current = next;
	}

	return res;
}

void OSThread::UpdateInfo(union sigval) {
    if (!started_) { return; }
    
    OSThreadIterator it;
    while (it.HasNext()) {
        OSThread *t = it.Next();
        
        if (t->alive_) {
            bool res = PthreadAlive(t->join_id_);
            if (!res) { t->alive_ = false; }
//            else { t->name(); }
        }
    }
    
    std::lock_guard ml(*thread_lock_);
    if (!started_) { return; }
    
    DeleteDeadThreadUnlock();
}

bool OSThread::RegisterTimer() {
    int rv;
    struct sigevent evp;
    memset(&evp, 0, sizeof(struct sigevent));
    evp.sigev_notify = SIGEV_THREAD;
    evp.sigev_notify_function = UpdateInfo;
    
    rv = timer_create(CLOCK_MONOTONIC, &evp, &update_timer_);
    
    if (rv) { return false; }
    
    struct itimerspec its;
    memset(&its, 0, sizeof(struct itimerspec));
    its.it_interval.tv_sec = kInfoUpdatePeriodS;
    its.it_interval.tv_nsec = 0;
    its.it_value = its.it_interval;
    rv = timer_settime(update_timer_, 0, &its, nullptr);
    
    if (rv) { return false; }
    
    return true;
}

OSThreadIterator::OSThreadIterator() {
	// Lock the thread list while iterating.
    OSThread::thread_lock_->lock();

	if (!OSThread::started_) { return; }

	os_thread_ = OSThread::thread_list_head_;
}

OSThreadIterator::~OSThreadIterator() {
	// Unlock the thread list when done.
    OSThread::thread_lock_->unlock();
}

bool OSThreadIterator::HasNext() const {
	if (!OSThread::started_) { return false; }

	return os_thread_ != nullptr;
}

OSThread *OSThreadIterator::Next() {
	if (!OSThread::started_) { return nullptr; }

	OSThread *current = os_thread_;
	os_thread_ = os_thread_->next_;

	return current;
}
}