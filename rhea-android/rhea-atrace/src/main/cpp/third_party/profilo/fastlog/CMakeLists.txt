CMAKE_MINIMUM_REQUIRED(VERSION 3.4.1)

PROJECT(fastlog)

set(TARGET fastlog)

set(
  ${TARGET}_SOURCE_FILES
  entries/Entry.cpp
  entries/EntryType.cpp
  logger/Logger.cpp
  logger/PacketLogger.cpp
  logger/buffer/RingBuffer.cpp
  logger/lfrb/Futex.cpp
  writer/TraceWriter.cpp
  writer/MultiTraceLifecycleVisitor.cpp
  writer/TraceLifecycleVisitor.cpp
  writer/PacketReassembler.cpp
  writer/trace_headers.cpp
  writer/trace_backwards.cpp
  writer/PrintEntryVisitor.cpp

  deps/fmt/fmt-6.1.2/src/format.cc
  deps/fmt/fmt-6.1.2/src/posix.cc
)

include_directories(deps)
#include_directories(deps/fmt/fmt-6.1.2/include)

add_definitions(-DNDEBUG)

add_library(
  ${TARGET}
  STATIC
  ${${TARGET}_SOURCE_FILES}
)

target_include_directories(
  ${TARGET}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/deps/fmt/fmt-6.1.2/include
)

find_library(log-lib log)
find_library(zlib z)

target_link_libraries(
  ${TARGET}
  PUBLIC ${zlib}
  PUBLIC ${log-lib}
)
