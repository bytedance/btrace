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

// Copyright (c) 2021, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef BTRACE_MEMORY_H_
#define BTRACE_MEMORY_H_

#ifdef __cplusplus

#include <mach/vm_param.h>

#include "utils.hpp"
#include "globals.hpp"
#include "assert.hpp"
#include "allocation.hpp"

namespace btrace
{

	class MemoryRegion : public ValueObject
	{
	public:
		MemoryRegion() : pointer_(nullptr), size_(0) {}
		MemoryRegion(void *pointer, uword size) : pointer_(pointer), size_(size) {}
		MemoryRegion(const MemoryRegion &other) : ValueObject() { *this = other; }
		MemoryRegion &operator=(const MemoryRegion &other)
		{
			pointer_ = other.pointer_;
			size_ = other.size_;
			return *this;
		}

		void *pointer() const { return pointer_; }
		uword size() const { return size_; }
		void set_size(uword new_size) { size_ = new_size; }

		uword start() const { return reinterpret_cast<uword>(pointer_); }
		uword end() const { return start() + size_; }

	private:
		void *pointer_;
		uword size_;
	};

	class VirtualMemory
	{
	public:
		//   // The reserved memory is unmapped on destruction.
		~VirtualMemory();

		uword start() const { return region_.start(); }
		//   uword end() const { return region_.end(); }
		void *address() const { return region_.pointer(); }
		//   intptr_t size() const { return region_.size(); }
		intptr_t AliasOffset() const { return alias_.start() - region_.start(); }

		static void Init();

		static VirtualMemory *Allocate(intptr_t size,
									   const char *name)
		{
			return AllocateAligned(size, PageSize(),
								   name);
		}
		static VirtualMemory *AllocateAligned(intptr_t size,
											  intptr_t alignment,
											  const char *name);

		static intptr_t PageSize()
		{
			ASSERT(page_size_ != 0);
			return page_size_;
		}

		bool vm_owns_region() const { return reserved_.pointer() != nullptr; }

	private:
		static intptr_t CalculatePageSize();

		VirtualMemory(const MemoryRegion &region, const MemoryRegion &reserved)
			: region_(region), alias_(region), reserved_(reserved) {}

		MemoryRegion region_;

		MemoryRegion alias_;
        
		MemoryRegion reserved_;

		static inline uword page_size_ = PAGE_SIZE;
		//   DISALLOW_IMPLICIT_CONSTRUCTORS(VirtualMemory);
	};

} // namespace btrace

#endif

#endif // BTRACE_MEMORY_H_
