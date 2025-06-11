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

#include "trace_backwards.h"

#include <entries/EntryParser.h>
#include <logger/buffer/RingBuffer.h>
#include <writer/PacketReassembler.h>

namespace facebook {
namespace profilo {
namespace writer {

void traceBackwards(
    entries::EntryVisitor& visitor,
    TraceBuffer& buffer,
    TraceBuffer::Cursor& cursor) {
  PacketReassembler reassembler([&visitor](const void* data, size_t size) {
    entries::EntryParser::parse(data, size, visitor);
  });

  TraceBuffer::Cursor backCursor{cursor};
  backCursor.moveBackward(); // Move back before trace start

  alignas(4) Packet packet;
  while (buffer.tryRead(packet, backCursor)) {
    reassembler.processBackwards(packet);
    if (!backCursor.moveBackward()) {
      break; // done
    }
  }
}

} // namespace writer
} // namespace profilo
} // namespace facebook
