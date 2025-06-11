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

#include <functional>
#include <list>
#include <vector>

namespace facebook {
namespace profilo {

using namespace logger;

namespace writer {
namespace detail {

struct PacketStream {
  StreamID stream;
  std::vector<char> data;

  PacketStream() = default;
  PacketStream(PacketStream&& other) = default;
  PacketStream(PacketStream const& other) = delete;
  PacketStream& operator=(PacketStream const& other) = delete;
  PacketStream& operator=(PacketStream&& other) = default;
};

} // namespace detail

class PacketReassembler {
 public:
  using PayloadCallback = std::function<void(const void*, size_t)>;

  PacketReassembler(PayloadCallback callback);
  void process(Packet const& packet);
  void processBackwards(Packet const& packet);

 private:
  static constexpr auto kStreamPoolSize = 8;

  std::list<detail::PacketStream> active_streams_;
  std::list<detail::PacketStream> pooled_streams_;
  PayloadCallback callback_;

  detail::PacketStream newStream();
  void startNewStream(Packet& packet);
  void recycleStream(detail::PacketStream stream);
};

} // namespace writer
} // namespace profilo
} // namespace facebook
