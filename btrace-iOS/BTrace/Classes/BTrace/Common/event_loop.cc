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
 */

#include <algorithm>

#include "utils.hpp"
#include "event_loop.hpp"

namespace btrace
{

    void EventLoop::Init()
    {
        ASSERT(!initialized_);
        if (entry_list == nullptr)
        {
            entry_list = new std::vector<Event>();
        }
        initialized_ = true;
        shutdown_ = true;
    }

    void EventLoop::Start()
    {
        ASSERT(initialized_);
        ASSERT(processor_thread_id_ == OSThread::kInvalidThreadJoinId);
        
        if (monitor_ == nullptr)
        {
            monitor_ = new Monitor();
        }
        ASSERT(monitor_ != nullptr);
        
        shutdown_ = false;
        
        MonitorLocker startup_ml(monitor_);
        OSThread::New("BTrace Event Loop", ThreadMain, 0);
        while (!thread_running_)
        {
            startup_ml.Wait(1);
        }
        ASSERT(processor_thread_id_ != OSThread::kInvalidThreadJoinId);
    }

    void EventLoop::Stop()
    {
        {
            MonitorLocker shutdown_ml(monitor_);
            if (shutdown_)
            {
                // Already shutdown.
                return;
            }
            shutdown_ = true;
            // Notify.
            shutdown_ml.Notify();
            ASSERT(initialized_);
        }

        // Join the thread.
        if (processor_thread_id_)
        {
            OSThread *thread = OSThread::Current();
            if (!thread || thread->joinId() != processor_thread_id_)
            {
                OSThread::Join(processor_thread_id_);
            }
            else
            {
                OSThread::Detach(processor_thread_id_);
            }
        }
        processor_thread_id_ = OSThread::kInvalidThreadJoinId;
        delete monitor_;
        monitor_ = nullptr;
        entry_list->clear();
        ASSERT(!thread_running_);
    }

    void EventLoop::ThreadMain(uword parameters)
    {
        if (!initialized_)
        {
            return;
        }
        
        OSThread::DisableLogging(false);
        
        {
            // Signal to main thread we are ready.
            MonitorLocker startup_ml(monitor_);
            OSThread *os_thread = OSThread::Current();
            ASSERT(os_thread != nullptr);
            os_thread->DisableInterrupts();
            processor_thread_id_ = os_thread->joinId();
            thread_running_ = true;
            startup_ml.Notify();
        }

        intptr_t count = 0;
        while (true)
        {
            if (unlikely(shutdown_))
            {
                break;
            }

            MonitorLocker wait_ml(monitor_);
            
            for (auto &entry: *entry_list) {
                if (count % entry.interval == 0)
                {
                    entry.fn();
                }
            }
            
            wait_ml.WaitMicros(base_period_ * kMicrosPerMilli);
            count += 1;
        }
        // Signal to main thread we are exiting.
        thread_running_ = false;
    }

    void EventLoop::Register(int period, Func fn) {
        int interval = std::max(period / base_period_, 1);
        entry_list->emplace_back(interval, fn);
    }
} // namespace btrace
