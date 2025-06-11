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

// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef BTRACE_THREAD_INTERRUPTER_H_
#define BTRACE_THREAD_INTERRUPTER_H_

#ifdef __cplusplus

#include "os_thread.hpp"
#include "monitor.hpp"

namespace btrace
{

	class ThreadInterrupter : public AllStatic
	{
	public:
		static void Init();

		static void Startup();
		static void Cleanup();

		// Delay between interrupts.
		static void SetPeriod(intptr_t period, intptr_t bg_period);

		// Interrupt a thread.
		static void InterruptThread(OSThread *thread);

	private:
		static inline bool initialized_ = false;
		static inline bool shutdown_ = false;
		static inline bool thread_running_ = false;
		static inline bool woken_up_ = false;
		static inline ThreadJoinId interrupter_thread_id_ =
			OSThread::kInvalidThreadJoinId;
		static inline Monitor *monitor_ = nullptr;
		static inline intptr_t interrupt_period_ = 1000; // unit: us
        static inline intptr_t interrupt_bg_period_ = 1000; // unit: us
		static inline float cpu_usage_threshold_ = 0.0;
        static inline uint32_t cpu_time_threshold_ = 10; // unit: us
		static inline intptr_t interrupted_thread_count_ = 0;

		static void ThreadMain(uword parameters);

		friend class ThreadInterrupterVisitor;
	};

} // namespace btrace

#endif

#endif // BTRACE_THREAD_INTERRUPTER_H_
