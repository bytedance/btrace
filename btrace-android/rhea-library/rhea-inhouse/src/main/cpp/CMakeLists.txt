cmake_minimum_required(VERSION 3.4.1)

set(RHEA_SRCS
        sampling/SamplingCollector.cpp
        sampling/SamplingConfig.cpp
        sampling/Stack.cpp
        sampling/StackVisitor.cpp
        stat/JavaObjectStat.cpp
        trace/SamplingTrace.cpp
        trace/TraceBinderCall.cpp
        trace/TraceGC.cpp
        trace/TraceJNICall.cpp
        trace/TraceMessageIDChange.cpp
        trace/TraceJavaMonitor.cpp
        trace/TraceLoadLibrary.cpp
        trace/TraceObjectWait.cpp
        trace/TraceUnsafePark.cpp
        trace/java_alloc/checkpoint.cpp
        trace/java_alloc/thread_list.cpp
        trace/java_alloc/TraceJavaAlloc.cpp
        utils/JNIHook.cpp
        utils/npth_dl.c
        TraceGlobalJni.cpp
        TraceAbilityJni.cpp
        RheaOnLoad.cpp
        RheaContext.cpp
        )

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-exceptions -fno-rtti")

set(TARGET rheatrace)

add_library(${TARGET} SHARED ${RHEA_SRCS})

target_include_directories(${TARGET} PUBLIC ${INCLUDE_DIRS})


find_package(shadowhook REQUIRED CONFIG)
target_link_libraries(${TARGET} log)
target_link_libraries(${TARGET} android)
target_link_libraries(${TARGET} shadowhook::shadowhook)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DRHEA_DEBUG)
endif ()