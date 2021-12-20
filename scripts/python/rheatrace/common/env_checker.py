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

import os
import sys
from enhanced_systrace import systrace_env
from rhea_atrace.rhea_log.rhea_logger import rhea_logger

logger = rhea_logger


def check_env():
    systrace_exe = systrace_env.get_executable_systrace()
    if systrace_exe is None:
        sys.stderr.write("Can't find systrace in environment path \n")
        return False
    adb_path = _find_adb()
    if adb_path is None:
        sys.stderr.write("Can't find adb in environment path \n")
        return False
    systrace_dir = os.path.dirname(systrace_exe)
    _add_systrace_dir_to_python_path(systrace_dir)
    _add_systrace_dir_to_python_path(os.path.join(systrace_dir, "catapult"))
    _add_systrace_dir_to_python_path(os.path.join(systrace_dir, "catapult", "systrace"))
    _add_systrace_dir_to_python_path(os.path.join(systrace_dir, "catapult", "devil"))
    return True


def check_python():
    version = sys.version_info[:2]
    if version != (2, 7):
        logger.error("RheaTrace does not support Python %d.%d. Please use Python 2.7.\n", version[0], version[1])
        return False
    return True


def check_systrace_path():
    """Finds systrace on the path."""
    paths = os.environ["PATH"].split(os.pathsep)
    systrace = "systrace"
    for p in paths:
        systrace_dir = os.path.join(p, systrace)
        if os.path.isdir(systrace_dir):
            return systrace_dir
    return None


def _find_adb():
    """Finds adb on the path."""
    paths = os.environ['PATH'].split(os.pathsep)
    executable = 'adb'
    if sys.platform == 'win32':
        executable = executable + '.exe'
    for p in paths:
        f = os.path.join(p, executable)
        if os.path.isfile(f):
            return f
    return None


def _add_systrace_dir_to_python_path(*path_parts):
    path = os.path.abspath(os.path.join(*path_parts))
    if os.path.isdir(path) and path not in sys.path:
        # Some callsite that use telemetry assumes that sys.path[0] is the directory
        # containing the script, so we add these extra paths to right after it.
        sys.path.insert(1, path)
