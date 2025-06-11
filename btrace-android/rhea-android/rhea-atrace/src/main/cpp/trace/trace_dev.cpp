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

#include "trace.h"

#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <utils/debug.h>
#include <utils/xcc_fmt.h>

#include "atrace.h"
#include "trace_provider.h"

/**
 * Maximum size of a message that can be logged to the trace buffer.
 * Note this message includes a tag, the pid, and the string given as the name.
 * Names should be kept short to get the most use of the trace buffer.
 */
#define ATRACE_MESSAGE_LENGTH 1024

#define WRITE_MSG(format_begin, format_end, name, value) {                           \
    if (!bytedance::atrace::ATrace::Get().IsATraceStarted()) {                       \
      return;                                                                        \
    }                                                                                \
    if (bytedance::atrace::TraceProvider::Get().isMainThreadOnly()                 \
     && bytedance::atrace::TraceProvider::Get().GetMainThreadId() != gettid()) {    \
    return;                                                                          \
    }                                                                                \
    char buf[ATRACE_MESSAGE_LENGTH];                                    \
    int pid = getpid();                                                 \
    int len = xcc_fmt_snprintf(buf, sizeof(buf), format_begin "%s" format_end,  \
                       pid, name, value);                               \
    if (len >= (int) sizeof(buf)) {                                     \
      /* Given the sizeof(buf), and all of the current format buffers,  \
       * it is impossible for name_len to be < 0 if len >= sizeof(buf). */ \
      int name_len = strlen(name) - (len - sizeof(buf)) - 1;            \
      /* Truncate the name to make the message fit. */                  \
      ALOGW("Truncated name in %s: %s\n", __FUNCTION__, name);          \
      len = xcc_fmt_snprintf(buf, sizeof(buf), format_begin "%.*s" format_end,  \
                     pid, name_len, name, value);                       \
    }                                                                   \
    bytedance::atrace::ATrace::Get().LogTrace(buf, len);                \
}

void atrace_begin_body(const char *name) {
  WRITE_MSG("B|%d|", "%s", name, "");
}

void atrace_begin_body_with_value(const char *name, const char *value) {
  WRITE_MSG("B|%d|", "%s", name, value);
}

void atrace_end_body() {
    WRITE_MSG("E|%d", "%s", "", "");
}
