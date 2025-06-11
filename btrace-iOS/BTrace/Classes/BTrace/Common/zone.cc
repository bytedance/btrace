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

#include <mutex>

#include "zone.hpp"

#include "assert.hpp"
#include "utils.hpp"
#include "memory.hpp"

namespace btrace
{

	std::atomic<intptr_t> Zone::total_size_ = {0};

	// Zone segments represent chunks of memory: They have starting
	// address encoded in the this pointer and a size in bytes. They are
	// chained together to form the backing storage for an expanding zone.
	class Zone::Segment
	{
	public:
		Segment *next() const { return next_; }
		intptr_t size() const { return size_; }
		VirtualMemory *memory() const { return memory_; }

		uword start() { return address(sizeof(Segment)); }
		uword end() { return address(size_); }

		// Allocate or delete individual segments.
		static Segment *New(intptr_t size, Segment *next);
		static void DeleteSegmentList(Segment *segment);

	private:
		Segment *next_;
		intptr_t size_;
		VirtualMemory *memory_;
		void *alignment_;

		// Computes the address of the nth byte in this segment.
		uword address(intptr_t n) { return reinterpret_cast<uword>(this) + n; }

		DISALLOW_IMPLICIT_CONSTRUCTORS(Segment);
	};

	static constexpr intptr_t kSegmentCacheCapacity = 16; // 1 MB of Segments
    static std::mutex *segment_cache_mutex = nullptr;
	static VirtualMemory *segment_cache[kSegmentCacheCapacity] = {nullptr};
	static intptr_t segment_cache_size = 0;

	void Zone::Init()
	{
        if (segment_cache_mutex == nullptr) {
            // do not delete segment_cache_mutex
            segment_cache_mutex = new std::mutex();
        }
	}

	void Zone::Stop()
	{
		ClearCache();
	}

	void Zone::ClearCache()
	{
		std::lock_guard<std::mutex> ml(*segment_cache_mutex);
		ASSERT(segment_cache_size >= 0);
		ASSERT(segment_cache_size <= kSegmentCacheCapacity);
		while (segment_cache_size > 0)
		{
			delete segment_cache[--segment_cache_size];
		}
	}

	Zone::Segment *Zone::Segment::New(intptr_t size, Zone::Segment *next)
	{
		size = Utils::RoundUp(size, VirtualMemory::PageSize());
		VirtualMemory *memory = nullptr;
		if (size == kSegmentSize)
		{
			std::lock_guard<std::mutex> ml(*segment_cache_mutex);
			ASSERT(segment_cache_size >= 0);
			ASSERT(segment_cache_size <= kSegmentCacheCapacity);
			if (segment_cache_size > 0)
			{
				memory = segment_cache[--segment_cache_size];
			}
		}
		if (memory == nullptr)
		{
			memory = VirtualMemory::Allocate(size, "btrace-zone");
			total_size_.fetch_add(size);
		}
		if (memory == nullptr)
		{
			OUT_OF_MEMORY();
		}
		Segment *result = reinterpret_cast<Segment *>(memory->start());
		result->next_ = next;
		result->size_ = size;
		result->memory_ = memory;
		result->alignment_ = nullptr; // Avoid unused variable warnings.

		return result;
	}

	void Zone::Segment::DeleteSegmentList(Segment *head)
	{
		Segment *current = head;
		while (current != nullptr)
		{
			intptr_t size = current->size();
			Segment *next = current->next();
			VirtualMemory *memory = current->memory();

			if (size == kSegmentSize)
			{
				std::lock_guard<std::mutex> ml(*segment_cache_mutex);
				ASSERT(segment_cache_size >= 0);
				ASSERT(segment_cache_size <= kSegmentCacheCapacity);
				if (segment_cache_size < kSegmentCacheCapacity)
				{
					segment_cache[segment_cache_size++] = memory;
					memory = nullptr;
				}
			}
			if (memory != nullptr)
			{
				total_size_.fetch_sub(size);
				delete memory;
			}
			current = next;
		}
	}

	Zone::Zone()
		: position_(reinterpret_cast<uword>(&buffer_)),
		  limit_(position_ + kInitialChunkSize),
		  segments_(nullptr),
		  previous_(nullptr)
	{
	}

	Zone::~Zone()
	{
		Segment::DeleteSegmentList(segments_);
	}

	uintptr_t Zone::SizeInBytes() const
	{
		return size_;
	}

	uintptr_t Zone::CapacityInBytes() const
	{
		uintptr_t size = kInitialChunkSize;
		for (Segment *s = segments_; s != nullptr; s = s->next())
		{
			size += s->size();
		}
		return size;
	}

	uword Zone::AllocateExpand(intptr_t size)
	{
		ASSERT(size >= 0);
		// Make sure the requested size is already properly aligned and that
		// there isn't enough room in the Zone to satisfy the request.
		ASSERT(Utils::IsAligned(size, kAlignment));
		intptr_t free_size = (limit_ - position_);
		ASSERT(free_size < size);

		// First check to see if we should just chain it as a large segment.
		intptr_t max_size =
			Utils::RoundDown(kSegmentSize - sizeof(Segment), kAlignment);
		ASSERT(max_size > 0);
		if (size > max_size)
		{
			return AllocateLargeSegment(size);
		}

		const intptr_t kSuperPageSize = 2 * MB;
		intptr_t next_size;
		if (small_segment_capacity_ < kSuperPageSize)
		{
			// When the Zone is small, grow linearly to reduce size and use the segment
			// cache to avoid expensive mmap calls.
			next_size = kSegmentSize;
		}
		else
		{
			// When the Zone is large, grow geometrically to avoid Page Table Entry
			// exhaustion. Using 1.125 ratio.
			next_size = Utils::RoundUp(small_segment_capacity_ >> 3, kSuperPageSize);
		}
		ASSERT(next_size >= kSegmentSize);

		// Allocate another segment and chain it up.
		segments_ = Segment::New(next_size, segments_);
		small_segment_capacity_ += next_size;

		// Recompute 'position' and 'limit' based on the new head segment.
		uword result = Utils::RoundUp(segments_->start(), kAlignment);
		position_ = result + size;
		limit_ = segments_->end();
		size_ += size;
		ASSERT(position_ <= limit_);
		return result;
	}

	uword Zone::AllocateLargeSegment(intptr_t size)
	{
		ASSERT(size >= 0);
		// Make sure the requested size is already properly aligned and that
		// there isn't enough room in the Zone to satisfy the request.
		ASSERT(Utils::IsAligned(size, kAlignment));
		intptr_t free_size = (limit_ - position_);
		ASSERT(free_size < size);

		// Create a new large segment and chain it up.
		// Account for book keeping fields in size.
		size_ += size;
		size += Utils::RoundUp(sizeof(Segment), kAlignment);
		segments_ = Segment::New(size, segments_);

		uword result = Utils::RoundUp(segments_->start(), kAlignment);
		return result;
	}

	StackZone::StackZone()
		: zone_()
	{
		Zone *lzone = GetZone();
		lzone->Link(OSThread::zone());
		OSThread::set_zone(lzone);
	}

	StackZone::~StackZone()
	{
		//  ASSERT(Thread::Current()->MayAllocateHandles());

		Zone *lzone = GetZone();
		ASSERT(OSThread::zone() == lzone);
		OSThread::set_zone(lzone->previous_);
	}

} // namespace btrace
