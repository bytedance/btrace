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

#pragma once

#include <stdlib.h>

#define MIN_FILE_COUNT 0

namespace bytedance {
namespace atrace {

ssize_t proxy_read(int fd, void* buf, size_t count);
ssize_t proxy_write(int fd, const void* buf, size_t count);

ssize_t proxy_read_chk(int fd, void* buf, size_t count, size_t buf_size);
ssize_t proxy_write_chk(int fd, const void* buf, size_t count, size_t buf_size);

ssize_t proxy_readv(int fd, struct inovec *iov, int invcnt);
ssize_t proxy_writev(int fd, const struct inovec *iov, int invcnt);

ssize_t proxy_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t proxy_pwrite(int fd, const void *buf, size_t count, off_t offset);

void proxy_sync(void);
int proxy_fsync(int fd);
int proxy_fdatasync(int fd);
int proxy_open(char *path, int flags, int mode);

}  // namespace atrace
}  // namespace bytedance
