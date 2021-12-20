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

#include <logger/lfrb/LockFreeRingBuffer.h>

namespace facebook {
namespace profilo {
namespace logger {

using StreamID = uint32_t;

struct __attribute__((packed)) Packet {
  constexpr static auto kPacketIdNone = 0;
  constexpr static auto kVersion = 2;

  StreamID stream;
  bool start : 1;
  bool next : 1;
  uint16_t size : 14;

  alignas(4) char data[52];
};

//
// We go through the template indirection in order to have meaningful failure
// messages that show the actual size.
//
template <typename ToCheck, std::size_t RealSize = sizeof(ToCheck)>
struct check_size {
  static_assert(
      RealSize % 64 == 0,
      "Size must be a multiple of the cache line size");
};
struct check_size_0 {
  check_size<lfrb::detail::RingBufferSlot<Packet, std::atomic>> check;
};

} // namespace logger
} // namespace profilo
} // namespace facebook
