/**
 * Copyright 2004-present, Facebook, Inc.
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

#pragma once

#include <logger/buffer/Packet.h>
#include <logger/lfrb/LockFreeRingBuffer.h>

#define PROFILOEXPORT __attribute__((visibility("default")))

namespace facebook {
namespace profilo {

using TraceBuffer = logger::lfrb::LockFreeRingBuffer<logger::Packet>;
using TraceBufferSlot = logger::lfrb::detail::RingBufferSlot<logger::Packet>;
using TraceBufferHolder =
    logger::lfrb::LockFreeRingBufferHolder<logger::Packet>;

class RingBuffer {
  static const size_t DEFAULT_SLOT_COUNT = 1000;

  PROFILOEXPORT static TraceBuffer& init(TraceBufferHolder* new_buffer);

 public:
  constexpr static auto kVersion = 1;

  //
  // sz - number of buffer slots
  //
  PROFILOEXPORT static TraceBuffer& init(size_t sz = DEFAULT_SLOT_COUNT);
  //
  // Constructs the buffer at the specified address
  // (for example in a memory mapped file)
  // sz - number of buffer slots
  // ptr - pointer to where allocate the buffer
  //
  PROFILOEXPORT static TraceBuffer& init(
      void* ptr,
      size_t sz = DEFAULT_SLOT_COUNT);

  //
  // Cleans-up current buffer and reverts back to no-op mode.
  // DO NOTE USE: This operation is unsafe and currently serve merely as stub
  // for future dynamic buffer management extensions. All tracing should be
  // disabled before this method can be called.
  //
  PROFILOEXPORT static void destroy();

  PROFILOEXPORT static TraceBuffer& get();
};

} // namespace profilo
} // namespace facebook
