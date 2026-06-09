/*
 * Copyright (c) 2026 Bytedance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created on 2025/10/13.
//

#include "TraceHooks.h"
#include "StackTrace.h"
#include <bits/alltypes.h>
#include <cerrno>
#include <cstddef>
#include <hilog/log.h>
#include <sys/mman.h>

#include "util/globals.h"
#include "util/hook_utils.h"

#undef LOG_TAG
#define LOG_TAG "btrace:Hooks"

namespace btrace {

constexpr uint32_t X_ALLOC_FLAG = 0x1;
constexpr uint32_t X_MEM_OP_FLAG = 0x2;
constexpr uint32_t X_STR_OP_FLAG = 0x4;
constexpr uint32_t X_FILE_OP_FLAG = 0x8;


static bool inited = false;

class ScopedHookAction {
public:
    static void bindAction(TraceHooks::Action f) {
        action_ = f;
    }
    
    ScopedHookAction(Type type, void* fp) : type_(type), fp_(fp) {}
    ~ScopedHookAction() {
        int err = errno;
        errno = 0;
        action_(type_, fp_);
        errno = err;
    }
private:
    static inline TraceHooks::Action action_;
    
    Type type_;
    void* fp_;
};

template<Type kType, class Fp>
class FunctionHooker;

template<Type kType, class Rp, class... ArgTypes>
class FunctionHooker<kType, Rp(ArgTypes...)> {
public:
    static void hook(const char* name) {
//        OH_LOG_INFO(LOG_APP, "hooking %{public}s", name);
        hook_all(name, (void *)proxy, (void **)&origin);
    }
private: 
    using F = Rp(*)(ArgTypes...);
    static Rp proxy(ArgTypes... args) {
        ScopedHookAction sha(kType, __builtin_frame_address(0));
        return reinterpret_cast<F>(origin)(args...);
    }
    static void* origin;

    FunctionHooker() = delete;
    ~FunctionHooker() = delete;
};

#define FUNCTION_HOOKER_GENERATOR(alias_name, type, sig) \
using alias_name = FunctionHooker<type, sig>; \
template<> void* alias_name::origin = nullptr;

FUNCTION_HOOKER_GENERATOR(MallocHooker, Type::kMalloc, void*(size_t))
FUNCTION_HOOKER_GENERATOR(CallocHooker, Type::kCalloc, void*(size_t, size_t))
FUNCTION_HOOKER_GENERATOR(ReallocHooker, Type::kRealloc, void*(void*, size_t))
FUNCTION_HOOKER_GENERATOR(FreeHooker, Type::kFree, void(void*))
FUNCTION_HOOKER_GENERATOR(MmapHooker, Type::kMmap, void*(void*, size_t, int, int, int, off_t))
FUNCTION_HOOKER_GENERATOR(MadviseHooker, Type::kMadvise, int(void*, size_t, int))
FUNCTION_HOOKER_GENERATOR(MsyncHooker, Type::kMsync, int(void*, size_t, int))
FUNCTION_HOOKER_GENERATOR(MunmapHooker, Type::kMunmap, int(void*, size_t))
FUNCTION_HOOKER_GENERATOR(MemsetHooker, Type::kMemset, void*(void*, int, size_t))
FUNCTION_HOOKER_GENERATOR(MemcpyHooker, Type::kMemcpy, void*(void*, const void*, size_t))
FUNCTION_HOOKER_GENERATOR(MemcmpHooker, Type::kMemcmp, int(const void*, const void*, size_t))
FUNCTION_HOOKER_GENERATOR(MemmemHooker, Type::kMemmem, void*(void*, size_t, void*, size_t))
FUNCTION_HOOKER_GENERATOR(MemmoveHooker, Type::kMemmove, void*(void*, const void*, size_t))
FUNCTION_HOOKER_GENERATOR(StrstrHooker, Type::kStrstr, char*(char*, const char*))
FUNCTION_HOOKER_GENERATOR(StrchrHooker, Type::kStrchr, char*(char*, int ))
FUNCTION_HOOKER_GENERATOR(StrlenHooker, Type::kStrlen, size_t(const char*))
FUNCTION_HOOKER_GENERATOR(OpenHooker, Type::kOpen, int(const char*, int, mode_t))
FUNCTION_HOOKER_GENERATOR(CloseHooker, Type::kClose, int(int))
FUNCTION_HOOKER_GENERATOR(ReadHooker, Type::kRead, ssize_t(int, void*, size_t))
FUNCTION_HOOKER_GENERATOR(WriteHooker, Type::kWrite, ssize_t(int, const void*, size_t))

TraceHooks& TraceHooks::get() {
    static TraceHooks inst{};
    return inst;
}

bool TraceHooks::setup(const HookConfig& config, Action hookAction) {
    if (inited) {
        return true;
    }

    hook_init();

    ScopedHookAction::bindAction(hookAction);
    int32_t magic = config.getMagic();
    if (magic >= 0) {
        MallocHooker::hook("malloc");
        if ((magic & X_ALLOC_FLAG) == 0) {
            CallocHooker::hook("calloc");
            ReallocHooker::hook("realloc");
        }
        FreeHooker::hook("free");
        MmapHooker::hook("mmap");
        MadviseHooker::hook("madvise");
        MsyncHooker::hook("msync");
        MunmapHooker::hook("munmap");
        if ((magic & X_MEM_OP_FLAG) == 0) {
            MemsetHooker::hook("memset");
            MemcpyHooker::hook("memcpy");
            MemcmpHooker::hook("memcmp");
            MemmemHooker::hook("memmem");
            MemmoveHooker::hook("memmove");
        }
        if ((magic & X_STR_OP_FLAG) == 0) {
            StrstrHooker::hook("strstr");
            StrchrHooker::hook("strchr");
            StrlenHooker::hook("strlen");
        }
        if ((magic & X_FILE_OP_FLAG) == 0) {
            OpenHooker::hook("open");
            CloseHooker::hook("close");
        }
        ReadHooker::hook("read");
        WriteHooker::hook("write");
    }

    inited = true;
    return true;
}

bool TraceHooks::cleanup() {
    return true;
}

}
