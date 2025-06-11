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

#ifndef BTRACE_ZONE_VECTOR_H_
#define BTRACE_ZONE_VECTOR_H_

#ifdef __cplusplus

#include <initializer_list>

#include "allocation.hpp"
#include "utils.hpp"
#include "zone.hpp"
#include "os_thread.hpp"

namespace btrace
{
  template <typename T, typename B, typename Allocator>
	class BaseVector : public B
	{
	public:
		explicit BaseVector(Allocator *allocator)
			: length_(0), capacity_(0), data_(nullptr), allocator_(allocator) {}

		BaseVector(intptr_t initial_capacity, Allocator *allocator)
			: length_(0), capacity_(0), data_(nullptr), allocator_(allocator)
		{
			if (initial_capacity > 0)
			{
				capacity_ = Utils::RoundUpToPowerOfTwo(initial_capacity);
				data_ = allocator_->template Alloc<T>(capacity_);
			}
		}

		BaseVector(BaseVector &&other)
			: length_(other.length_),
			  capacity_(other.capacity_),
			  data_(other.data_),
			  allocator_(other.allocator_)
		{
			other.length_ = 0;
			other.capacity_ = 0;
			other.data_ = nullptr;
		}

		~BaseVector()
		{ /*allocator_->template Free<T>(data_, capacity_);*/
		}

		BaseVector &operator=(BaseVector &&other)
		{
			intptr_t temp = other.length_;
			other.length_ = length_;
			length_ = temp;
			temp = other.capacity_;
			other.capacity_ = capacity_;
			capacity_ = temp;
			T *temp_data = other.data_;
			other.data_ = data_;
			data_ = temp_data;
			Allocator *temp_allocator = other.allocator_;
			other.allocator_ = allocator_;
			allocator_ = temp_allocator;
			return *this;
		}

		intptr_t length() const { return length_; }
		T *data() const { return data_; }
		bool is_empty() const { return length_ == 0; }

		void Add(const T &value)
		{
			Resize(length() + 1);
			Last() = value;
		}

		T &operator[](intptr_t index) const
		{
			ASSERT(0 <= index);
			ASSERT(index < length_);
			ASSERT(length_ <= capacity_);
			return data_[index];
		}

		const T &At(intptr_t index) const { return operator[](index); }

		T &Last() const
		{
			ASSERT(length_ > 0);
			return operator[](length_ - 1);
		}

		void Clear() { length_ = 0; }

		void InsertAt(intptr_t idx, const T &value)
		{
			Resize(length() + 1);
			for (intptr_t i = length_ - 2; i >= idx; i--)
			{
				data_[i + 1] = data_[i];
			}
			data_[idx] = value;
		}

		// The content (if expanded) is uninitialized after calling it.
		// The backing store (if expanded) will grow with by a power-of-2.
		void Resize(intptr_t new_length);

		T *begin() { return &data_[0]; }
		const T *begin() const { return &data_[0]; }

		T *end() { return &data_[length_]; }
		const T *end() const { return &data_[length_]; }

	private:
		intptr_t length_;
		intptr_t capacity_;
		T *data_;
		Allocator *allocator_; // Used to (re)allocate the array.

		DISALLOW_COPY_AND_ASSIGN(BaseVector);
	};

	template <typename T, typename B, typename Allocator>
	void BaseVector<T, B, Allocator>::Resize(intptr_t new_length)
	{
		if (new_length > capacity_)
		{
			intptr_t new_capacity = Utils::RoundUpToPowerOfTwo(new_length);
			T *new_data =
				allocator_->template Realloc<T>(data_, capacity_, new_capacity);
			ASSERT(new_data != nullptr);
			data_ = new_data;
			capacity_ = new_capacity;
		}
		length_ = new_length;
	}

  template <typename T>
  class ZoneVector : public BaseVector<T, ZoneAllocated, Zone>
  {
  public:
    ZoneVector(Zone *zone, intptr_t initial_capacity)
        : BaseVector<T, ZoneAllocated, Zone>(initial_capacity,
                                                    ASSERT_NOTNULL(zone)) {}
    explicit ZoneVector(intptr_t initial_capacity)
        : BaseVector<T, ZoneAllocated, Zone>(
              initial_capacity,
              ASSERT_NOTNULL(OSThread::zone())) {}
    ZoneVector()
        : BaseVector<T, ZoneAllocated, Zone>(
              ASSERT_NOTNULL(OSThread::zone())) {}

    ZoneVector(ZoneVector &&other) = default;
    ZoneVector &operator=(ZoneVector &&other) = default;
    ~ZoneVector() = default;
    DISALLOW_COPY_AND_ASSIGN(ZoneVector);
  };
} // namespace btrace

#endif

#endif // BTRACE_ZONE_VECTOR_H_
