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

#ifndef BTRACE_ASSERT_H_
#define BTRACE_ASSERT_H_

#ifdef __cplusplus

#include "globals.hpp"

#if defined(DEBUG)
#include <sstream>
#include <string>
#endif

namespace btrace
{

	class DynamicAssertionHelper
	{
	public:
		DynamicAssertionHelper(const char *file, int line)
			: file_(file), line_(line) {}

	protected:
		void Print(const char *format,
				   va_list arguments,
				   bool will_abort = false) const;

		const char *const file_;
		const int line_;

		DISALLOW_IMPLICIT_CONSTRUCTORS(DynamicAssertionHelper);
	};

	class Assert : public DynamicAssertionHelper
	{
	public:
		Assert(const char *file, int line) : DynamicAssertionHelper(file, line) {}

		BTRACE_NORETURN void Fail(const char *format, ...) const PRINTF_ATTRIBUTE(2, 3);

		template <typename T>
		T NotNull(const T p);
	};

	template <typename T>
	T Assert::NotNull(const T p)
	{
		if (p != nullptr)
			return p;
		Fail("expected: not nullptr, found nullptr");
		return nullptr;
	}

} // namespace btrace

#define UNREACHABLE() FATAL("unreachable code")

#define OUT_OF_MEMORY() FATAL("Out of memory.")

#if defined(DEBUG)

#define FATAL(format, ...) \
    btrace::Assert(__FILE__, __LINE__).Fail(format, ##__VA_ARGS__);

#define ASSERT(cond)                                                           \
	do                                                                         \
	{                                                                          \
		if (!(cond))                                                           \
			btrace::Assert(__FILE__, __LINE__).Fail("expected: %s", #cond); \
	} while (false)

#define ASSERT_NOTNULL(ptr) btrace::Assert(__FILE__, __LINE__).NotNull((ptr))

#else // if defined(DEBUG)

#define FATAL(format, ...) \
    do                     \
    {                      \
    } while (false)

#define ASSERT(condition) \
	do                    \
	{                     \
	} while (false && (condition))

#define ASSERT_NOTNULL(ptr) (ptr)

#endif // if defined(DEBUG)

#endif // ifdef __cplusplus

#endif  // BTRACE_ASSERT_H_
