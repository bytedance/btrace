/*
 * Copyright (C) 2005 The Android Open Source Project
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

#include "utils/timers.h"

#include <time.h>

#if defined(__linux__)
nsecs_t systemTime(int clock)
{
  static const clockid_t clocks[] = {
    CLOCK_REALTIME,
    CLOCK_MONOTONIC,
    CLOCK_PROCESS_CPUTIME_ID,
    CLOCK_THREAD_CPUTIME_ID,
    CLOCK_BOOTTIME
  };
  struct timespec t;
  t.tv_sec = t.tv_nsec = 0;
  clock_gettime(clocks[clock], &t);
  return nsecs_t(t.tv_sec)*1000000000LL + t.tv_nsec;
}
#else
nsecs_t systemTime(int /*clock*/)
{
  // Clock support varies widely across hosts. Mac OS doesn't support
  // CLOCK_BOOTTIME, and Windows is windows.
  struct timeval t;
  t.tv_sec = t.tv_usec = 0;
  gettimeofday(&t, nullptr);
  return nsecs_t(t.tv_sec)*1000000000LL + nsecs_t(t.tv_usec)*1000LL;
}
#endif

/*
 * native public static long uptimeMillis();
 */
int64_t uptimeMillis()
{
  int64_t when = systemTime(SYSTEM_TIME_MONOTONIC);
  return (int64_t) nanoseconds_to_milliseconds(when);
}

/*
 * native public static long elapsedRealtimeMillis();
 */
int64_t elapsedRealtimeMillis()
{
  int64_t when = systemTime(SYSTEM_TIME_BOOTTIME);
  return (int64_t) nanoseconds_to_milliseconds(when);
}

/*
 * native public static long elapsedRealtimeMillis();
 */
int64_t elapsedRealtimeMicros()
{
  int64_t when = systemTime(SYSTEM_TIME_BOOTTIME);
  return (int64_t) nanoseconds_to_microseconds(when);
}
