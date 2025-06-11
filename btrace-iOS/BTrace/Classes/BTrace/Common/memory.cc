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

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "globals.hpp"
#include "memory.hpp"

#include "assert.hpp"
#include "utils.hpp"

namespace btrace
{

#undef MAP_FAILED
#define MAP_FAILED reinterpret_cast<void *>(-1)

	static bool Trace_dual_map_code = false;
	static bool Trace_write_protect_code = false;

	static void *Map(void *addr,
					 size_t length,
					 int prot,
					 int flags,
					 int fd,
					 off_t offset)
	{
		void *result = mmap(addr, length, prot, flags, fd, offset);
		int error = errno;
		if ((result == MAP_FAILED) && (error != ENOMEM))
		{
			const int kBufferSize = 1024;
			char error_buf[kBufferSize];
			FATAL("mmap failed: %d (%s)", error,
				  Utils::StrError(error, error_buf, kBufferSize));
		}
		return result;
	}

	static void Unmap(uword start, uword end)
	{
		ASSERT(start <= end);
		uword size = end - start;
		if (size == 0)
		{
			return;
		}

		if (munmap(reinterpret_cast<void *>(start), size) != 0)
		{
			int error = errno;
			const int kBufferSize = 1024;
			char error_buf[kBufferSize];
			FATAL("munmap failed: %d (%s)", error,
				  Utils::StrError(error, error_buf, kBufferSize));
		}
	}

	static void *GenericMapAligned(void *hint,
								   int prot,
								   intptr_t size,
								   intptr_t alignment,
								   intptr_t allocated_size,
								   int map_flags)
	{
		void *address = Map(hint, allocated_size, prot, map_flags, -1, 0);
		if (address == MAP_FAILED)
		{
			return nullptr;
		}

		const uword base = reinterpret_cast<uword>(address);
		const uword aligned_base = Utils::RoundUp(base, alignment);

		Unmap(base, aligned_base);
		Unmap(aligned_base + size, base + allocated_size);
		return reinterpret_cast<void *>(aligned_base);
	}

    intptr_t VirtualMemory::CalculatePageSize()
    {
        const intptr_t page_size = getpagesize();
        ASSERT(page_size != 0);
        return page_size;
    }

    void VirtualMemory::Init() {
        page_size_ = CalculatePageSize();
    }

	VirtualMemory *VirtualMemory::AllocateAligned(intptr_t size,
												  intptr_t alignment,
												  const char *name)
	{
		ASSERT(Utils::IsAligned(size, PageSize()));
		ASSERT(Utils::IsAligned(alignment, PageSize()));
		ASSERT(name != nullptr);

		const intptr_t allocated_size = size + alignment - PageSize();

		const int prot = PROT_READ | PROT_WRITE;

		int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;

		void *hint = nullptr;
		void *address =
			GenericMapAligned(hint, prot, size, alignment, allocated_size, map_flags);
		if (address == nullptr)
		{
			return nullptr;
		}

		MemoryRegion region(reinterpret_cast<void *>(address), size);
		return new VirtualMemory(region, region);
	}

	VirtualMemory::~VirtualMemory()
	{
		if (vm_owns_region())
		{
			Unmap(reserved_.start(), reserved_.end());
			const intptr_t alias_offset = AliasOffset();
			if (alias_offset != 0)
			{
				Unmap(reserved_.start() + alias_offset, reserved_.end() + alias_offset);
			}
		}
	}

} // namespace btrace
