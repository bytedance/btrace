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

#include "PacketReassembler.h"

#include <algorithm>

namespace facebook {
namespace profilo {
namespace writer {

using detail::PacketStream;

PacketReassembler::PacketReassembler(
    PacketReassembler::PayloadCallback callback)
    : active_streams_(),
      pooled_streams_(kStreamPoolSize),
      callback_(std::move(callback)) {}

namespace {

inline void appendToStream(PacketStream& stream, Packet const& packet) {
  auto prev_size = stream.data.size();
  auto next_size = prev_size + packet.size;
  stream.data.resize(next_size);

  char* data = static_cast<char*>(stream.data.data());
  std::memcpy(data + prev_size, packet.data, packet.size);
}

inline void appendToStreamReverse(PacketStream& stream, Packet const& packet) {
  auto prev_size = stream.data.size();
  auto next_size = prev_size + packet.size;
  stream.data.resize(next_size);

  char* data = static_cast<char*>(stream.data.data());
  std::memcpy(data + prev_size, packet.data, packet.size);
  std::reverse(data + prev_size, data + prev_size + packet.size);
}

} // anonymous namespace

PacketStream PacketReassembler::newStream() {
  if (!pooled_streams_.empty()) {
    // Take a pooled stream and remove the moved-from instance.
    PacketStream stream = std::move(*pooled_streams_.begin());
    pooled_streams_.pop_front();
    return stream;
  } else {
    // No pooled stream to use, make a new one.
    return PacketStream{};
  }
}

void PacketReassembler::recycleStream(PacketStream stream) {
  // Return to pool, if necessary. Otherwise, release via RAII.
  if (pooled_streams_.size() < kStreamPoolSize) {
    // Changes the `size` to 0 but the `capacity` will not be affected.
    stream.data.resize(0);
    pooled_streams_.push_back(std::move(stream));
  }
}

void PacketReassembler::process(Packet const& packet) {
  //
  // Collect packets into active_streams_, inside PacketStream objects.
  //
  // Last packet within the stream flushes to the callback.
  // The PacketStream object can then be moved to pooled_stream_ for reuse.
  //

  // Is this part of an existing stream?
  if (active_streams_.size() > 0) {
    for (auto it = active_streams_.begin(), end = active_streams_.end();
         it != end;
         ++it) {
      auto& stream = *it;

      if (stream.stream == packet.stream) {
        appendToStream(stream, packet);

        if (!packet.next) {
          // Flush the stream
          callback_(stream.data.data(), stream.data.size());

          PacketStream temp_stream = std::move(*it);
          active_streams_.erase(it); // remove moved-from instance

          recycleStream(std::move(temp_stream));
        }
        return; // packet is handled
      }
    }
  }

  if (packet.start && !packet.next) {
    callback_(packet.data, packet.size);
  } else if (packet.start) { // Ignore if we only started from the middle of the
                             // packet
    PacketStream stream = newStream();
    stream.stream = packet.stream;
    appendToStream(stream, packet);
    active_streams_.push_front(std::move(stream));
  }
}

void PacketReassembler::processBackwards(Packet const& packet) {
  //
  // Collect packets into active_streams_, inside PacketStream objects.
  //
  // Last packet within the stream flushes to the callback.
  // The PacketStream object can then be moved to pooled_stream_ for reuse.
  //

  // Is this part of an existing stream?
  if (active_streams_.size() > 0) {
    for (auto it = active_streams_.begin(), end = active_streams_.end();
         it != end;
         ++it) {
      auto& stream = *it;

      if (stream.stream == packet.stream) {
        appendToStreamReverse(stream, packet);

        if (packet.start) {
          // Flush the stream
          std::reverse(stream.data.begin(), stream.data.end());
          callback_(stream.data.data(), stream.data.size());

          PacketStream temp_stream = std::move(*it);
          active_streams_.erase(it); // remove moved-from instance

          recycleStream(std::move(temp_stream));
        }
        return; // packet is handled
      }
    }
  }

  if (packet.start && !packet.next) {
    callback_(packet.data, packet.size);
  } else if (!packet.next) { // Ignore if we only started from the middle of the
                             // packet
    PacketStream stream = newStream();
    stream.stream = packet.stream;
    appendToStreamReverse(stream, packet);
    active_streams_.push_front(std::move(stream));
  }
}

} // namespace writer
} // namespace profilo
} // namespace facebook
