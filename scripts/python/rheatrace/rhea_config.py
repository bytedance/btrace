#!/usr/bin/env python

# Copyright (C) 2021 ByteDance Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import logging

VERSION_CODE = "1.1.0"

ORIGIN_SYSTRACE_FILE = "systrace-origin.html"
ORIGIN_SYSTRACE_FS_FILE = 'systrace-fs-origin.html'
ATRACE_APP_GZ_FILE_LOCATION = "sdcard/rhea-trace/%s/"
ATRACE_GZ_FILE = 'rhea-atrace.gz'
ATRACE_BINDER_FILE = 'binder.txt'
ATRACE_RAW_FILE = 'rhea-atrace'
ATRACE_STANDARDIZED_FILE = 'atrace-standard'
ATRACE_SYS_FILE = 'atrace-sys'

DEFAULT_OUTPUT_FILE = "systrace.html"

ENVIRONMENT_PARAMS_ANDROID_FS = "enable_android_fs"

RHEA_TRACE_ACTION_SWITCH_START = "com.bytedance.rheatrace.switch.start"
RHEA_TRACE_ACTION_SWITCH_STOP = "com.bytedance.rheatrace.switch.stop"

LOG_IS_OPEN = True
LOG_LEVEL = logging.INFO
LOG_IS_SAVE = True
