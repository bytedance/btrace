/*
 * Copyright (C) 2021 ByteDance Inc
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
 */

#pragma once

#include <string.h>
#include <sys/types.h>

#if __cplusplus >= 201103L

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete; \
    void operator=(const TypeName&) = delete

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
    TypeName() = delete; \
    DISALLOW_COPY_AND_ASSIGN(TypeName)

#else

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&); \
    void operator=(const TypeName&)

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
    TypeName(); \
    DISALLOW_COPY_AND_ASSIGN(TypeName)

#endif

/* CPP header guards */
#ifdef __cplusplus
#define __BEGIN_CDECLS  extern "C" {
#define __END_CDECLS    }
#else
#define __BEGIN_CDECLS
#define __END_CDECLS
#endif

/*
 * Needed for cases where something should be constexpr if possible, but not
 * being constexpr is fine if in pre-C++11 code (such as a const static float
 * member variable).
 */
#if __cplusplus >= 201103L
#define CONSTEXPR constexpr
#else
#define CONSTEXPR
#endif
