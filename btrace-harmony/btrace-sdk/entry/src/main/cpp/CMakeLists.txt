# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(harmony_btrace_demo)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH})

add_library(entry SHARED napi_init.cpp)
target_link_libraries(entry PUBLIC libace_napi.z.so libdeviceinfo_ndk.z.so libhilog_ndk.z.so libohhidebug.so libc++_shared.so)