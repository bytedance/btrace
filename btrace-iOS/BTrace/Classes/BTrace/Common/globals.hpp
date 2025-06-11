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

#ifndef BTRACE_GLOBALS_H_
#define BTRACE_GLOBALS_H_

#ifdef __cplusplus

#include <unistd.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef __cplusplus

#include <cassert> // For assert() in constant expressions.

#endif

#if defined(__APPLE__)

// Define the flavor of Mac OS we are running on.
#include <TargetConditionals.h>
#define BTRACE_HOST_OS_MACOS 1
#if TARGET_OS_IPHONE
#define BTRACE_HOST_OS_IOS 1
#else
#error Automatic target os detection failed.
#endif
#endif

namespace btrace
{

#if defined(_M_ARM64) || defined(__aarch64__)
#define HOST_ARCH_ARM64 1
#define ARCH_IS_64_BIT 1
#else
#error Architecture was not detected.
#endif

#ifdef __GNUC__
#define BTRACE_FORCE_INLINE inline __attribute__((always_inline))
#else
#error Automatic compiler detection failed.
#endif

#ifdef __GNUC__
#define BTRACE_NOINLINE __attribute__((noinline))
#else
#error Automatic compiler detection failed.
#endif

#ifdef __GNUC__
#define BTRACE_NORETURN __attribute__((noreturn))
#else
#error Automatic compiler detection failed.
#endif

#if !defined(TARGET_ARCH_X64) && !defined(TARGET_ARCH_ARM64)
#if defined(HOST_ARCH_X64)
#define TARGET_ARCH_X64 1
#elif defined(HOST_ARCH_ARM64)
#define TARGET_ARCH_ARM64 1
#else
#error Automatic target architecture detection failed.
#endif
#endif

#if defined(TARGET_ARCH_X64) || defined(TARGET_ARCH_ARM64)
#define TARGET_ARCH_IS_64_BIT 1
#else
#error Automatic target architecture detection failed.
#endif

#if !defined(BTRACE_TARGET_OS_MACOS_IOS) && \
    !defined(BTRACE_TARGET_OS_MACOS)
#if defined(BTRACE_HOST_OS_IOS)
#define BTRACE_TARGET_OS_MACOS 1
#define BTRACE_TARGET_OS_MACOS_IOS 1
#elif defined(BTRACE_HOST_OS_MACOS)
#define BTRACE_TARGET_OS_MACOS 1
#else
#error Automatic target OS detection failed.
#endif
#endif

// Short form printf format specifiers
#define Pd PRIdPTR
#define Pu PRIuPTR

    constexpr uint64_t NANOS_PER_USEC = 1000ULL;
    constexpr uint64_t NANOS_PER_MILLISEC = 1000ULL * NANOS_PER_USEC;

    // Byte sizes.
    constexpr int kInt64SizeLog2 = 3;

    constexpr int kDoubleSize = sizeof(double);

    // Bit sizes.
    constexpr int kBitsPerByteLog2 = 3;

    // Integer constants.
    constexpr int32_t kMaxInt32 = 0x7FFFFFFF;

    typedef intptr_t word;
    typedef uintptr_t uword;

// Byte sizes for native machine words.
#ifdef ARCH_IS_32_BIT
    constexpr int kWordSizeLog2 = kInt32SizeLog2;
#else
    constexpr int kWordSizeLog2 = kInt64SizeLog2;
#endif
    constexpr int kWordSize = 1 << kWordSizeLog2;
    static_assert(kWordSize == sizeof(word), "Mismatched word size constant");

    // Bit sizes for native machine words.
    constexpr int kBitsPerWordLog2 = kWordSizeLog2 + kBitsPerByteLog2;
    constexpr int kBitsPerWord = 1 << kBitsPerWordLog2;

    // System-wide named constants.
    constexpr intptr_t KBLog2 = 10;
    constexpr intptr_t KB = 1 << KBLog2;
    constexpr intptr_t MBLog2 = KBLog2 + KBLog2;
    constexpr intptr_t MB = 1 << MBLog2;

    constexpr intptr_t kIntptrOne = 1;
    constexpr intptr_t kIntptrMin = (kIntptrOne << (kBitsPerWord - 1));
    constexpr intptr_t kIntptrMax = ~kIntptrMin;

    // Time constants.
    constexpr int kMillisPerSec = 1000;
    constexpr int kMicrosPerMilli = 1000;
    constexpr int kMicrosPerSec = kMicrosPerMilli * kMillisPerSec;
    constexpr int kNanosPerMicro = 1000;
    constexpr int kNanosPerMilli = kNanosPerMicro * kMicrosPerMilli;
    constexpr int kNanosecondsPerSecond = kNanosPerMicro * kMicrosPerSec;

#if DEBUG || INHOUSE_TARGET || TEST_MODE || READING_DEV
    constexpr uint32_t kMaxStackDepth = 256;
#else
    constexpr uint32_t kMaxStackDepth = 192;
#endif


#if !defined(DISALLOW_COPY_AND_ASSIGN)
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
private:                                   \
    TypeName(const TypeName &) = delete;   \
    void operator=(const TypeName &) = delete
#endif // !defined(DISALLOW_COPY_AND_ASSIGN)

#if !defined(DISALLOW_IMPLICIT_CONSTRUCTORS)
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
private:                                         \
    TypeName() = delete;                         \
    DISALLOW_COPY_AND_ASSIGN(TypeName)
#endif // !defined(DISALLOW_IMPLICIT_CONSTRUCTORS)

#if !defined(DISALLOW_ALLOCATION)
#define DISALLOW_ALLOCATION()                  \
public:                                        \
    void operator delete(void *pointer)        \
    {                                          \
        fprintf(stderr, "unreachable code\n"); \
        abort();                               \
    }                                          \
                                               \
private:                                       \
    void *operator new(size_t size);
#endif // !defined(DISALLOW_ALLOCATION)

#define ALIGN8 __attribute__((aligned(8)))

#if __GNUC__
#define PRINTF_ATTRIBUTE(string_index, first_to_check) \
    __attribute__((__format__(__printf__, string_index, first_to_check)))
#else
#define PRINTF_ATTRIBUTE(string_index, first_to_check)
#endif

#define ARRAY_SIZE(array)                 \
    ((sizeof(array) / sizeof(*(array))) / \
     static_cast<intptr_t>(!(sizeof(array) % sizeof(*(array))))) // NOLINT

} // namespace btrace

#endif

#endif // BTRACE_GLOBALS_H_
