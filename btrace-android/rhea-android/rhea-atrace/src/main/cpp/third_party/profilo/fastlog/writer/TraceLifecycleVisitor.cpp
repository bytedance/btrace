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

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <zlib.h>
#include <sstream>
#include <system_error>

#include <writer/PrintEntryVisitor.h>
#include <writer/TraceLifecycleVisitor.h>

#define LOG_TAG "Rhea"
#include "debug.h"

namespace facebook {
namespace profilo {
namespace writer {

using namespace facebook::profilo::entries;

namespace {

std::string getTraceID(int64_t trace_id) {
  const char* kBase64Alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";
  const size_t kTraceIdStringLen = 11;

  if (trace_id < 0) {
    throw std::invalid_argument("trace_id must be non-negative");
  }
  char result[kTraceIdStringLen + 1]{};
  for (ssize_t idx = kTraceIdStringLen - 1; idx >= 0; --idx) {
    result[idx] = kBase64Alphabet[trace_id % 64];
    trace_id /= 64;
  }
  return std::string(result);
}

std::string getTraceFilename(
    const std::string& trace_prefix,
    const std::string& trace_id) {
  std::stringstream filename;
  filename << trace_prefix << "-" << getpid() << "-";

  auto now = time(nullptr);
  struct tm localnow {};
  if (localtime_r(&now, &localnow) == nullptr) {
    throw std::runtime_error("Could not localtime_r(3)");
  }

  filename << (1900 + localnow.tm_year) << "-" << (1 + localnow.tm_mon) << "-"
           << localnow.tm_mday << "T" << localnow.tm_hour << "-"
           << localnow.tm_min << "-" << localnow.tm_sec;

  filename << "-" << trace_id << ".tmp";
  return filename.str();
}

std::string sanitize(std::string input) {
  for (size_t idx = 0; idx < input.size(); ++idx) {
    char ch = input[idx];
    bool is_valid = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.';

    if (!is_valid) {
      input[idx] = '_';
    }
  }
  return input;
}

// Creates the directory specified by a path, creating intermediate
// directories as needed
void mkdirs(char const* dir) {
  auto len = strlen(dir);
  char partial[len + 1];
  memset(partial, 0, len + 1);
  strncpy(partial, dir, len);
  struct stat s {};
  char* delim{};

  // Iterate our path backwards until we find a string we can stat(), which
  // is an indicator of an existent directory
  while (stat(partial, &s)) {
    delim = strrchr(partial, '/');
    *delim = 0;
  }

  // <partial> now contains a path to a directory that actually exists, so
  // create all the intermediate directories until we finally have the
  // file system hierarchy specified by <dir>
  while (delim != nullptr && delim < partial + len) {
    *delim = '/';
    delim = strchr(delim + 1, '\0');
    if (mkdirat(0, partial, S_IRWXU | S_IRWXG)) {
      if (errno != EEXIST) {
        std::stringstream ss;
        ss << "Could not mkdirat() folder ";
        ss << partial;
        ss << ", errno = ";
        ss << strerror(errno);
        throw std::system_error(errno, std::system_category(), ss.str());
      }
    }
  }
}

void ensureFolder(const char* folder) {
  struct stat stat_out {};
  if (stat(folder, &stat_out)) {
    if (errno != ENOENT) {
      std::string error = std::string("Could not stat() folder ") + folder;
      throw std::system_error(errno, std::system_category(), error);
    }

    // errno == ENOENT, folder needs creating
    mkdirs(folder);
  }
}

} // namespace

TraceLifecycleVisitor::TraceLifecycleVisitor(
    const std::string& folder,
    const std::string& trace_prefix,
    std::shared_ptr<TraceCallbacks> callbacks,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int64_t trace_id)
    :

      folder_(folder),
      trace_prefix_(trace_prefix),
      trace_headers_(headers),
      output_(nullptr),
      delegates_(),
      expected_trace_(trace_id),
      callbacks_(callbacks),
      done_(false) {}

void TraceLifecycleVisitor::visit(const StandardEntry& entry) {
  auto type = static_cast<EntryType>(entry.type);
  switch (type) {
    case EntryType::TRACE_END: {
      int64_t trace_id = entry.extra;
      if (trace_id != expected_trace_) {
        return;
      }
      // write before we clean up state
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      onTraceEnd(trace_id);
      break;
    }
    case EntryType::TRACE_TIMEOUT:
    case EntryType::TRACE_ABORT: {
      int64_t trace_id = entry.extra;
      if (trace_id != expected_trace_) {
        return;
      }
      auto reason = type == EntryType::TRACE_TIMEOUT
          ? AbortReason::TIMEOUT
          : AbortReason::CONTROLLER_INITIATED;

      // write before we clean up state
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      onTraceAbort(trace_id, reason);
      break;
    }
    case EntryType::TRACE_BACKWARDS:
    case EntryType::TRACE_START: {
      onTraceStart(entry.extra, entry.matchid);
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      break;
    }
    case EntryType::LOGGER_PRIORITY: {
      if (expected_trace_ == entry.extra) {
        thread_priority_ = std::make_unique<ScopedThreadPriority>(entry.callid);
      }
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      break;
    }
    default: {
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
    }
  }
}

void TraceLifecycleVisitor::visit(const FramesEntry& entry) {
  if (hasDelegate()) {
    delegates_.back()->visit(entry);
  }
}

void TraceLifecycleVisitor::visit(const BytesEntry& entry) {
  if (hasDelegate()) {
    delegates_.back()->visit(entry);
  }
}

void TraceLifecycleVisitor::abort(AbortReason reason) {
  onTraceAbort(expected_trace_, reason);
}

void TraceLifecycleVisitor::onTraceStart(int64_t trace_id, int32_t flags) {
  if (trace_id != expected_trace_) {
    return;
  }

  if (output_ != nullptr) {
    // active trace with same ID, abort
    abort(AbortReason::NEW_START);
    return;
  }

  std::stringstream path_stream;
  std::string trace_id_string = getTraceID(trace_id);
  //path_stream << folder_ << '/' << sanitize(trace_id_string);
  path_stream << folder_;

  //
  // Note that the construction of this path must match the computation in
  // TraceOrchestrator.getSanitizedTraceFolder. Unfortunately, it's far too
  // gnarly to share this code at the moment.
  //
  std::string trace_folder = path_stream.str();
  try {
    ensureFolder(trace_folder.c_str());
  } catch (const std::system_error& ex) {
    // Add more diagnostics to the exception.
    // Namely, parent folder owner uid and gid, as
    // well as our own uid and gid.
    struct stat stat_out {};

    std::stringstream error;
    if (stat(folder_.c_str(), &stat_out)) {
      error << "Could not stat(" << folder_
            << ").\nOriginal exception: " << ex.what();
      throw std::system_error(errno, std::system_category(), error.str());
    }

    error << "Could not create trace folder " << trace_folder
          << ".\nOriginal exception: " << ex.what() << ".\nDebug info for "
          << folder_ << ": uid=" << stat_out.st_uid
          << "; gid=" << stat_out.st_gid << "; proc euid=" << geteuid()
          << "; proc egid=" << getegid();
    throw std::system_error(ex.code(), error.str());
  }

  //path_stream << '/'
  //            << sanitize(getTraceFilename(trace_prefix_, trace_id_string));
  path_stream << '/' << trace_prefix_ << ".gz";

  std::string trace_file = path_stream.str();
  ALOGW("trace file is: %s", trace_file.c_str());

  output_ = std::make_unique<std::ofstream>(
      trace_file, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
  output_->exceptions(std::ofstream::badbit | std::ofstream::failbit);

  zstr::ostreambuf* output_buf_ = new zstr::ostreambuf(
      output_->rdbuf(), // wrap the ofstream buffer
      512 * 1024, // input and output buffers
      3 // compression level
  );

  // Disable ofstream buffering
  output_->rdbuf()->pubsetbuf(nullptr, 0);
  // Replace ofstream buffer with the compressed one
  output_->basic_ios<char>::rdbuf(output_buf_);

  //writeHeaders(*output_, trace_id_string);

  // outputTime = truncate(current) - truncate(prev)
  delegates_.emplace_back(new PrintEntryVisitor(*output_));

  if (callbacks_.get() != nullptr) {
      callbacks_->OnTraceStart(trace_id, flags, trace_file);
  }

  done_ = false;
}

void TraceLifecycleVisitor::onTraceAbort(int64_t trace_id, AbortReason reason) {
  done_ = true;
  cleanupState();
  if (callbacks_.get() != nullptr) {
    callbacks_->OnTraceAbort(trace_id, reason);
  }
}

void TraceLifecycleVisitor::onTraceEnd(int64_t trace_id) {
  done_ = true;
  cleanupState();
  if (callbacks_.get() != nullptr) {
    callbacks_->OnTraceEnd(trace_id);
  }
}

void TraceLifecycleVisitor::cleanupState() {
  delegates_.clear();
  thread_priority_ = nullptr;
  output_->flush();
  output_->close();
  output_ = nullptr;
}

void TraceLifecycleVisitor::writeHeaders(std::ostream& output, std::string id) {
  output << "dt\n"
         << "ver|" << kTraceFormatVersion << "\n"
         << "id|" << id << "\n"
         << "prec|" << kTimestampPrecision << "\n";

  for (auto const& header : trace_headers_) {
    output << header.first << '|' << header.second << '\n';
  }

  output << '\n';
}

} // namespace writer
} // namespace profilo
} // namespace facebook
