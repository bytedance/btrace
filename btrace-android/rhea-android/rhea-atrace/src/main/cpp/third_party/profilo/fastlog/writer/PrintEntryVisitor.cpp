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

#include <writer/PrintEntryVisitor.h>
#include <fmt/format.h>

namespace facebook {
namespace profilo {
namespace writer {

PrintEntryVisitor::PrintEntryVisitor(std::ostream& stream) : stream_(stream) {}

void PrintEntryVisitor::visit(const StandardEntry& data) {
  stream_ << fmt::format_int{data.id}.c_str();
  stream_ << '|';
  stream_ << entries::to_string((EntryType)data.type);
  stream_ << '|';
  stream_ << fmt::format_int{data.timestamp}.c_str();
  stream_ << '|';
  stream_ << fmt::format_int{data.tid}.c_str();
  stream_ << '|';
  stream_ << fmt::format_int{data.callid}.c_str();
  stream_ << '|';
  stream_ << fmt::format_int{data.matchid}.c_str();
  stream_ << '|';
  stream_ << fmt::format_int{data.extra}.c_str();
  stream_ << '\n';
}

void PrintEntryVisitor::visit(const FramesEntry& data) {
  for (size_t idx = 0; idx < data.frames.size; ++idx) {
    stream_ << fmt::format_int{data.id}.c_str();
    stream_ << '|';
    stream_ << entries::to_string((EntryType)data.type);
    stream_ << '|';
    stream_ << fmt::format_int{data.timestamp}.c_str();
    stream_ << '|';
    stream_ << fmt::format_int{data.tid}.c_str();
    stream_ << "|0|";
    stream_ << fmt::format_int{data.matchid}.c_str();
    stream_ << '|';
    stream_ << fmt::format_int{data.frames.values[idx]}.c_str();
    stream_ << '\n';
  }
}

void PrintEntryVisitor::visit(const BytesEntry& data) {
  /*stream_ << fmt::format_int{data.id}.c_str();
  stream_ << '|';
  stream_ << entries::to_string((EntryType)data.type);
  stream_ << '|';
  stream_ << fmt::format_int{data.matchid}.c_str();
  stream_ << '|';*/
  {
    char _bytes_buf[data.bytes.size + 1];
    memset(_bytes_buf, 0, data.bytes.size + 1);
    memcpy(_bytes_buf, data.bytes.values, data.bytes.size);
    stream_ << _bytes_buf;
  }
  //stream_ << '\n';
}

} // namespace writer
} // namespace profilo
} // namespace facebook
