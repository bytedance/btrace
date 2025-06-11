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

#include "assert.hpp"
#include "globals.hpp"

namespace btrace
{

	void DynamicAssertionHelper::Print(const char *format,
									   va_list arguments,
									   bool will_abort /* = false */) const
	{
		// Take only the last 1KB of the file name if it is longer.
		const intptr_t file_len = strlen(file_);
		const intptr_t file_offset = (file_len > (1 * KB)) ? file_len - (1 * KB) : 0;
		const char *file = file_ + file_offset;

		// Print the file and line number into the buffer.
		char buffer[4 * KB];
		intptr_t file_and_line_length =
			snprintf(buffer, sizeof(buffer), "%s: %d: error: ", file, line_);

		// Print the error message into the buffer.
		vsnprintf(buffer + file_and_line_length,
				  sizeof(buffer) - file_and_line_length, format, arguments);

		// Print the buffer on stderr and/or syslog.
		fprintf(stderr, "%s\n", buffer);
		fflush(stderr);
	}

	void Assert::Fail(const char *format, ...) const
	{
		va_list arguments;
		va_start(arguments, format);
		Print(format, arguments, /*will_abort=*/true);
		va_end(arguments);

		abort();
	}

} // namespace btrace
