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

#include "allocation.hpp"

#include "assert.hpp"
#include "zone.hpp"

namespace btrace
{
	static void *Allocate(uword size, Zone *zone)
	{
		ASSERT(zone != nullptr);
		if (size > static_cast<uword>(kIntptrMax))
		{
			FATAL("ZoneAllocated object has unexpectedly large size %" Pu "", size);
		}
		return reinterpret_cast<void *>(zone->AllocUnsafe(size));
		return 0;
	}

	void *ZoneAllocated::operator new(uword size)
	{
		return Allocate(size, OSThread::zone());
	}

	void *ZoneAllocated::operator new(uword size, Zone *zone)
	{
		return Allocate(size, zone);
	}

} // namespace btrace
