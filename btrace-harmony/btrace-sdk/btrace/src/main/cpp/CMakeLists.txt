# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(myNpmLib)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

# 模块的oh_modules目录所在位置
set(OH_MODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../oh_modules)

# nativehook的头文件和动态库路径
set(NATIVEHOOK_LIB ${NATIVERENDER_ROOT_PATH}/../../../libs/${OHOS_ARCH}/libnative_hook.so)

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

add_library(btrace SHARED
            napi_init.cpp
            TraceConfigurations.cpp
            TraceAgent.cpp
            TraceBuffer.cpp
            TraceUnwinder.cpp
            ThreadLocalStorage.cpp
            TraceHooks.cpp
            TraceDumper.cpp
            ThreadNames.cpp
            util/thread_util.cpp
            util/time_utils.cpp
            util/common_utils.h
            util/common_utils.cpp
            OSThread.h
            OSThread.cpp
            util/globals.h
            util/globals.cpp
            util/memory_utils.h
            util/memory_utils.cpp
            util/lock_utils.h
            util/lock_utils.cpp
            SlowSyscallProxy.h
            SlowSyscallProxy.cpp
            RingBuffer.h
            RingBuffer.cpp
            CallstackTable.h
            CallstackTable.cpp
            TracePreloader.h
            TracePreloader.cpp
            util/hook_utils.cpp
            util/hook_utils.h)

target_link_libraries(btrace PUBLIC libace_napi.z.so libdeviceinfo_ndk.z.so libhilog_ndk.z.so libohhidebug.so libc++_shared.so)
target_link_libraries(btrace PUBLIC ${NATIVEHOOK_LIB})

