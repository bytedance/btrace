/*
 * Copyright (C) 2021 ByteDance Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Rhea.ioproxy"
#include <fcntl.h>
#include <utils/debug.h>
#include <utils/proc_fs.h>
#include <utils/strings.h>
#include <utils/timers.h>

#include "second_party/byte_hook/bytehook.h"
#include "trace_provider.h"
#include "atrace.h"
#include "trace.h"
#include "io_proxy.h"

namespace bytedance {
namespace atrace {

static std::string FileInfo(int fd, size_t count = 0, off_t offset = 0) {
    std::string str = utils::GetPath(fd);
    if (count > 0) {
        str.append(",size:");
        str.append(utils::to_string(count));
    }
    if (offset > 0) {
        str.append(",offset:");
        str.append(utils::to_string(offset));
    }
    return str;
}

// write family
ssize_t proxy_write(int fd, const void* buf, size_t count) {
  BYTEHOOK_STACK_SCOPE();

  if (ATrace::Get().IsATrace(fd, count)) {
    ATrace::Get().LogTrace(buf, count);
    return count;
  }

  if (count < MIN_FILE_COUNT || !TraceProvider::Get().IsEnableIO() ) {
    size_t ret = BYTEHOOK_CALL_PREV(proxy_write, fd, buf, count);
    return ret;
  }

  ATRACE_BEGIN_VALUE("write:", FileInfo(fd, count).c_str());
  size_t ret = BYTEHOOK_CALL_PREV(proxy_write, fd, buf, count);

  ATRACE_END();

  return ret;
}

ssize_t proxy_write_chk(int fd, const void* buf, size_t count, size_t buf_size) {
  BYTEHOOK_STACK_SCOPE();

  if (ATrace::Get().IsATrace(fd, count)) {
    ATrace::Get().LogTrace(buf, count);
    return count;
  }

  if (count < MIN_FILE_COUNT || !TraceProvider::Get().IsEnableIO()) {
    size_t ret = BYTEHOOK_CALL_PREV(proxy_write_chk, fd, buf, count, buf_size);
    return ret;
  }

  ATRACE_BEGIN_VALUE("__write_chk:", FileInfo(fd, count).c_str());

  size_t ret = BYTEHOOK_CALL_PREV(proxy_write_chk, fd, buf, count, buf_size);

  ATRACE_END();

  return ret;
}

ssize_t proxy_pwrite(int fd, const void *buf, size_t count, off_t offset) {
  if (count < MIN_FILE_COUNT || offset < 0 ) {
    size_t ret = BYTEHOOK_CALL_PREV(proxy_pwrite, fd, buf, count, offset);
    return ret;
  }
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN_VALUE("pwrite:", FileInfo(fd, count, offset).c_str());

  size_t ret = BYTEHOOK_CALL_PREV(proxy_pwrite, fd, buf, count, offset);

  ATRACE_END();
  return ret;
}

// read family
ssize_t proxy_read(int fd, void* buf, size_t count) {
  if (count < MIN_FILE_COUNT) {
    size_t ret = BYTEHOOK_CALL_PREV(proxy_read, fd, buf, count);
    return ret;
  }
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN_VALUE("read:", FileInfo(fd, count).c_str());

  size_t ret = BYTEHOOK_CALL_PREV(proxy_read, fd, buf, count);

  ATRACE_END();
  return ret;
}

ssize_t proxy_read_chk(int fd, void* buf, size_t count, size_t buf_size) {
  if (count < MIN_FILE_COUNT) {
    size_t ret = BYTEHOOK_CALL_PREV(proxy_read_chk, fd, buf, count, buf_size);
    return ret;
  }
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN_VALUE("__read_chk:", FileInfo(fd, count).c_str());

  size_t ret = BYTEHOOK_CALL_PREV(proxy_read_chk, fd, buf, count, buf_size);

  ATRACE_END();
  return ret;
}

ssize_t proxy_readv(int fd, struct inovec *iov, int iovcnt) {
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN_VALUE("readv:", FileInfo(fd).c_str());

  size_t ret = BYTEHOOK_CALL_PREV(proxy_readv, fd, iov, iovcnt);

  ATRACE_END();
  return ret;
}

ssize_t proxy_writev(int fd, const struct inovec *iov, int invcnt) {
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN_VALUE("writev:", FileInfo(fd).c_str());

  size_t ret = BYTEHOOK_CALL_PREV(proxy_writev, fd, iov, invcnt);

  ATRACE_END();
  return ret;
}

ssize_t proxy_pread(int fd, void *buf, size_t count, off_t offset) {
  if (count < MIN_FILE_COUNT || offset < 0) {
    size_t ret = BYTEHOOK_CALL_PREV(proxy_pread, fd, buf, count, offset);
    return ret;
  }
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN_VALUE("pread:", FileInfo(fd, count, offset).c_str());

  size_t ret = BYTEHOOK_CALL_PREV(proxy_pread, fd, buf, count, offset);

  ATRACE_END();
  return ret;
}

// sync family
void proxy_sync() {
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN("io:sync");

  BYTEHOOK_CALL_PREV(proxy_sync);

  ATRACE_END();
}

int proxy_fsync(int fd) {
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN_VALUE("fsync:", FileInfo(fd).c_str());

  int ret = BYTEHOOK_CALL_PREV(proxy_fsync, fd);

  ATRACE_END();
  return ret;
}

int proxy_fdatasync(int fd) {
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN_VALUE("fdatasync:", FileInfo(fd).c_str());

  int ret = BYTEHOOK_CALL_PREV(proxy_fdatasync, fd);

  ATRACE_END();
  return ret;
}

int proxy_open(char *path, int flags, int mode) {
  BYTEHOOK_STACK_SCOPE();
  ATRACE_BEGIN_VALUE("open:", path);

  int ret = BYTEHOOK_CALL_PREV(proxy_open, path, flags, mode);

  ATRACE_END();
  return ret;
}

}  // namespace atrace
}  // namespace bytedance
