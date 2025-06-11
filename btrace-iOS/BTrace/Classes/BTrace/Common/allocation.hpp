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

#ifndef BTRACE_ALLOCATION_H_
#define BTRACE_ALLOCATION_H_

#ifdef __cplusplus

#include "assert.hpp"
#include "globals.hpp"

namespace btrace
{

    // Forward declarations.
    class OSThread;
    class Zone;

    // void *malloc(size_t size);
    // void *realloc(void *ptr, size_t size);

    // Stack allocated objects subclass from this base class. Objects of this type
    // cannot be allocated on either the C or object heaps. Destructors for objects
    // of this type will not be run unless the stack is unwound through normal
    // program control flow.
    class ValueObject
    {
    public:
        ValueObject() {}
        ~ValueObject() {}

    private:
        DISALLOW_ALLOCATION();
        DISALLOW_COPY_AND_ASSIGN(ValueObject);
    };

    // Static allocated classes only contain static members and can never
    // be instantiated in the heap or on the stack.
    class AllStatic
    {
    private:
        DISALLOW_ALLOCATION();
        DISALLOW_IMPLICIT_CONSTRUCTORS(AllStatic);
    };
    
    // Zone allocated objects cannot be individually deallocated, but have
    // to rely on the destructor of Zone which is called when the Zone
    // goes out of scope to reclaim memory.
    class ZoneAllocated
    {
    public:
        ZoneAllocated() {}

        // Implicitly allocate the object in the current zone.
        void *operator new(size_t size);

        // Allocate the object in the given zone, which must be the current zone.
        void *operator new(size_t size, Zone *zone);

        // Ideally, the delete operator should be protected instead of
        // public, but unfortunately the compiler sometimes synthesizes
        // (unused) destructors for classes derived from ZoneObject, which
        // require the operator to be visible. MSVC requires the delete
        // operator to be public.

        // Disallow explicit deallocation of nodes. Nodes can only be
        // deallocated by invoking DeleteAll() on the zone they live in.
        void operator delete(void *pointer) { UNREACHABLE(); }

    private:
        DISALLOW_COPY_AND_ASSIGN(ZoneAllocated);
    };

} // namespace btrace

// Prevent use of `new (zone) DoesNotExtendZoneAllocated()`, which places the
// DoesNotExtendZoneAllocated on top of the Zone.
void *operator new(size_t size, btrace::Zone *zone) = delete;

#endif

#endif // BTRACE_ALLOCATION_H_
