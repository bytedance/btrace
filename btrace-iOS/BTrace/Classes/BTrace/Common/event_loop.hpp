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

#ifndef BTRACE_EVENTLOOP_H_
#define BTRACE_EVENTLOOP_H_

#ifdef __cplusplus

#include <atomic>
#include <vector>

#include "globals.hpp"
#include "monitor.hpp"
#include "os_thread.hpp"

// Profiler sampling and stack walking support.、
namespace btrace
{

    using Func = void(*)(void);

    struct Event {
        int interval;
        Func fn;
        
        Event(int interval, Func fn): interval(interval), fn(fn) {}
    };

	class EventLoop : public AllStatic
	{
	public:
        static void Init();
		static void Start();
		static void Stop();
        
        static Monitor *monitor() { return monitor_; };
        static void Register(int period, Func fn);

	private:
        static inline bool initialized_ = false;
		static inline bool shutdown_ = false;
		static inline bool thread_running_ = false;
		static inline ThreadJoinId processor_thread_id_ = OSThread::kInvalidThreadJoinId;
        static constexpr int base_period_ = 30; // unit: ms
		static inline Monitor *monitor_ = nullptr;
        static inline std::vector<Event> *entry_list = nullptr;
        
		static void ThreadMain(uword parameters);
	};

} // namespace btrace

#endif

#endif // BTRACE_EVENTLOOP_H_
