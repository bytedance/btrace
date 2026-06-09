/*
 * Copyright (c) 2026 Bytedance Inc.
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

//
// Created on 2026/4/1.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "memory_utils.h"

#include <cstring>
#include <sys/uio.h>
#include <unistd.h>

namespace btrace {
bool SafeReadMemory(void *src, void *dest, size_t len) {
    struct iovec local[1];
    local[0].iov_base = dest;
    local[0].iov_len = len;

    struct iovec remote[1];
    remote[0].iov_base = src;
    remote[0].iov_len = len;

    pid_t self = getpid();
    ssize_t res = process_vm_readv(self, local, 1, remote, 1, 0);

    if (res == -1) {
        return false;
    }

    return true;
}
}