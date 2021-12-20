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

#include "RingBuffer.h"
#include "../lfrb/LockFreeRingBuffer.h"

#include <atomic>
#include <memory>

#define LOG_TAG "Rhea.debug"
#include <debug.h>

namespace facebook {
namespace profilo {

namespace {

TraceBufferHolder noop_buffer = TraceBuffer::allocate(1);
std::atomic<TraceBufferHolder*> buffer(&noop_buffer);

} // namespace

TraceBuffer& RingBuffer::init(size_t sz) {
  if (buffer.load() != &noop_buffer) {
    // Already initialized
    return get();
  }

  return init(new TraceBufferHolder(TraceBuffer::allocate(sz)));
}

TraceBuffer& RingBuffer::init(void* ptr, size_t sz) {
  if (buffer.load() != &noop_buffer) {
    // Already initialized
    return get();
  }

  return init(new TraceBufferHolder(TraceBuffer::allocateAt(sz, ptr)));
}

TraceBuffer& RingBuffer::init(TraceBufferHolder* new_buffer) {
  if (buffer.load() != &noop_buffer) {
    // Already initialized
    return get();
  }

  auto expected = &noop_buffer;
  // We expect the update succeed only once for noop_buffer
  if (!buffer.compare_exchange_strong(expected, new_buffer)) {
    delete new_buffer;
  }

  return get();
}

void RingBuffer::destroy() {
  if (buffer.load() == &noop_buffer) {
    return;
  }

  auto expected = buffer.load();
  if (buffer.compare_exchange_strong(expected, &noop_buffer)) {
    delete expected;
  }
}

TraceBuffer& RingBuffer::get() {
  return **(buffer.load());
}

} // namespace profilo
} // namespace facebook
