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

#ifndef BTRACE_ZONE_H_
#define BTRACE_ZONE_H_

#ifdef __cplusplus

#include <atomic>
#include "utils.hpp"
#include "allocation.hpp"
#include "os_thread.hpp"

namespace btrace
{

	// Zones support very fast allocation of small chunks of memory. The
	// chunks cannot be deallocated individually, but instead zones
	// support deallocating all chunks in one fast operation.

	class Zone
	{
	public:
		Zone();
		// Allocate an array sized to hold 'len' elements of type
		// 'ElementType'.  Checks for integer overflow when performing the
		// size computation.
		~Zone(); // Delete all memory associated with the zone.
		template <class ElementType>
		inline ElementType *Alloc(intptr_t len);

		// Allocates an array sized to hold 'len' elements of type
		// 'ElementType'.  The new array is initialized from the memory of
		// 'old_array' up to 'old_len'.
		template <class ElementType>
		inline ElementType *Realloc(ElementType *old_array,
									intptr_t old_len,
									intptr_t new_len);

		// Allocates 'size' bytes of memory in the zone; expands the zone by
		// allocating new segments of memory on demand using 'new'.
		//
		// It is preferred to use Alloc<T>() instead, as that function can
		// check for integer overflow.  If you use AllocUnsafe, you are
		// responsible for avoiding integer overflow yourself.
		inline uword AllocUnsafe(intptr_t size);

		// Compute the total size of allocations in this zone.
		uintptr_t SizeInBytes() const;

		// Computes the amount of space used by the zone.
		uintptr_t CapacityInBytes() const;

		// Structure for managing handles allocation.
		//  VMHandles* handles() { return &handles_; }

		//  void VisitObjectPointers(ObjectPointerVisitor* visitor);

		Zone *previous() const { return previous_; }

		bool ContainsNestedZone(Zone *other) const
		{
			while (other != nullptr)
			{
				if (this == other)
					return true;
				other = other->previous_;
			}
			return false;
		}

		// All pointers returned from AllocateUnsafe() and New() have this alignment.
		static constexpr intptr_t kAlignment = kDoubleSize;

		static void Init();
		static void Stop();

		static void ClearCache();
		static intptr_t Size() { return total_size_; }

	private:
		// Default initial chunk size.
		static constexpr intptr_t kInitialChunkSize = 4 * KB;

		// Default segment size.
		static constexpr intptr_t kSegmentSize = 64 * KB;

		// Total size of current zone segments.
		static std::atomic<intptr_t> total_size_;

		// Expand the zone to accommodate an allocation of 'size' bytes.
		uword AllocateExpand(intptr_t size);

		// Allocate a large segment.
		uword AllocateLargeSegment(intptr_t size);

		// Insert zone into zone chain, after current_zone.
		void Link(Zone *current_zone) { previous_ = current_zone; }

		// Does not actually free any memory. Enables templated containers like
		// BaseVector to use different allocators.
		template <class ElementType>
		void Free(ElementType *old_array, intptr_t len)
		{
		}

		// Overflow check (FATAL) for array length.
		template <class ElementType>
		static inline void CheckLength(intptr_t len);

		// The free region in the current (head) segment or the initial buffer is
		// represented as the half-open interval [position, limit). The 'position'
		// variable is guaranteed to be aligned as dictated by kAlignment.
		uword position_;
		uword limit_;

		// Zone segments are internal data structures used to hold information
		// about the memory segmentations that constitute a zone. The entire
		// implementation is in zone.cc.
		class Segment;

		// Total size of all allocations in this zone.
		intptr_t size_ = 0;

		// Total size of all segments in [head_].
		intptr_t small_segment_capacity_ = 0;

		// List of all segments allocated in this zone; may be nullptr.
		Segment *segments_;

		// Used for chaining zones in order to allow unwinding of stacks.
		Zone *previous_;

		static_assert(kAlignment <= 8, "");
		ALIGN8 uint8_t buffer_[kInitialChunkSize];

		friend class StackZone;
		template <typename T, typename B, typename Allocator>
		friend class BaseVector;
		DISALLOW_COPY_AND_ASSIGN(Zone);
	};

	class StackZone
	{
	public:
		// Create an empty zone and set is at the current zone for the Thread.
		explicit StackZone();

		// Delete all memory associated with the zone.
		virtual ~StackZone();

		// Compute the total size of this zone. This includes wasted space that is
		// due to internal fragmentation in the segments.
		uintptr_t SizeInBytes() const
		{
			return zone_.SizeInBytes();
		}

		// Computes the used space in the zone.
		intptr_t CapacityInBytes() const
		{
			return zone_.CapacityInBytes();
		}

		Zone *GetZone()
		{
			return &zone_;
		}

	private:
		Zone zone_;
		template <typename T>
		friend class ZoneVector;
	};

	inline uword Zone::AllocUnsafe(intptr_t size)
	{
		ASSERT(size >= 0);
		// Round up the requested size to fit the alignment.
		if (size > (kIntptrMax - kAlignment))
		{
			FATAL("Zone::Alloc: 'size' is too large: size=%" Pd "", size);
		}
		size = Utils::RoundUp(size, kAlignment);

		// Check if the requested size is available without expanding.
		uword result;
		intptr_t free_size = (limit_ - position_);
		if (free_size >= size)
		{
			result = position_;
			position_ += size;
			size_ += size;
		}
		else
		{
			result = AllocateExpand(size);
		}

		// Check that the result has the proper alignment and return it.
		ASSERT(Utils::IsAligned(result, kAlignment));
		return result;
	}

	template <class ElementType>
	inline void Zone::CheckLength(intptr_t len)
	{
		const intptr_t kElementSize = sizeof(ElementType);
		if (len > (kIntptrMax / kElementSize))
		{
			FATAL("Zone::Alloc: 'len' is too large: len=%" Pd ", kElementSize=%" Pd,
				  len, kElementSize);
		}
	}

	template <class ElementType>
	inline ElementType *Zone::Alloc(intptr_t len)
	{
		CheckLength<ElementType>(len);
		return reinterpret_cast<ElementType *>(AllocUnsafe(len * sizeof(ElementType)));
	}

	template <class ElementType>
	inline ElementType *Zone::Realloc(ElementType *old_data,
									  intptr_t old_len,
									  intptr_t new_len)
	{
		CheckLength<ElementType>(new_len);
		const intptr_t kElementSize = sizeof(ElementType);
		if (old_data != nullptr)
		{
			uword old_end =
				reinterpret_cast<uword>(old_data) + (old_len * kElementSize);
			// Resize existing allocation if nothing was allocated in between...
			if (Utils::RoundUp(old_end, kAlignment) == position_)
			{
				uword new_end =
					reinterpret_cast<uword>(old_data) + (new_len * kElementSize);
				// ...and there is sufficient space.
				if (new_end <= limit_)
				{
					position_ = Utils::RoundUp(new_end, kAlignment);
					size_ += static_cast<intptr_t>(new_len - old_len);
					return old_data;
				}
			}
			if (new_len <= old_len)
			{
				return old_data;
			}
		}
		ElementType *new_data = Alloc<ElementType>(new_len);
		if (old_data != nullptr)
		{
			memmove(reinterpret_cast<void *>(new_data),
					reinterpret_cast<void *>(old_data), old_len * kElementSize);
		}
		return new_data;
	}

} // namespace btrace

#endif

#endif // BTRACE_ZONE_H_
