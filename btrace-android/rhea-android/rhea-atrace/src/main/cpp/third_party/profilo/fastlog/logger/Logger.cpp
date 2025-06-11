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

#include "Logger.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace facebook {
namespace profilo {

using namespace entries;

Logger& Logger::get() {
  static Logger logger(
      [&]() -> logger::PacketBuffer& { return RingBuffer::get(); },
      kInitialEntryId);
  return logger;
}

int32_t Logger::writeBytes(
    EntryType type,
    int32_t arg1,
    const uint8_t* arg2,
    size_t len) {
  if (len > kMaxVariableLengthEntry) {
    throw std::overflow_error("len is bigger than kMaxVariableLengthEntry");
  }
  if (arg2 == nullptr) {
    throw std::invalid_argument("arg2 is null");
  }

  BytesEntry entry{.id = 0,
                   .type = type,
                   .matchid = arg1,
                   .bytes = {.values = const_cast<uint8_t*>(arg2),
                             .size = static_cast<uint16_t>(len)}};

  return write(std::move(entry));
}

void Logger::writeStackFrames(
    int32_t tid,
    int64_t time,
    const int64_t* methods,
    uint8_t depth,
    int32_t matchid,
    EntryType entry_type) {
  FramesEntry entry{
      .id = 0,
      .type = entry_type,
      .timestamp = time,
      .tid = tid,
      .matchid = matchid,
      .frames = {.values = const_cast<int64_t*>(methods), .size = depth}};
  write(std::move(entry));
}

void Logger::writeTraceAnnotation(int32_t key, int64_t value) {
  write(StandardEntry{
      .id = 0,
      .type = EntryType::TRACE_ANNOTATION,
      .timestamp = 0, //monotonicTime(),
      .tid = 0, //threadID(),
      .callid = key,
      .matchid = 0,
      .extra = value,
  });
}

} // namespace profilo
} // namespace facebook
