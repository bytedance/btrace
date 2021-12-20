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

import sys
import io_extender
import rhea_config
from enhanced_systrace import systrace_env
from common import cmd_executer
from rhea_atrace.rhea_log.rhea_logger import rhea_logger

logger = rhea_logger


def capture(context):
    root_cmd = ["root"]
    (out, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(root_cmd, context.serial_number))
    open_android_fs = False
    if return_code is 0:
        if "cannot" not in out:
            logger.info("current devices is rooted!")
            _drop_cache()
            open_android_fs = __open_android_fs(context.serial_number)
    logger.debug("start to capture systrace.")
    (out, return_code) = __capture_systrace(context)
    logger.debug(out)
    if return_code is 0:
        context.set_params(rhea_config.ENVIRONMENT_PARAMS_ANDROID_FS, open_android_fs)
        if open_android_fs:
            io_extender.extend(context.get_build_file_path(rhea_config.ORIGIN_SYSTRACE_FILE),
                               context.get_build_file_path(rhea_config.ORIGIN_SYSTRACE_FS_FILE))
        return True
    logger.error("failed to capture systrace, check your inputs firstly.")
    return False


def show_list_categories(serial_number):
    systrace_path = systrace_env.get_executable_systrace()
    if systrace_path is None:
        logger.error("can't find systrace in system environment.")
        sys.exit(1)
    cmd = [systrace_path]
    cmd.extend(["-l"])
    if serial_number is not None:
        cmd.extend(["-e", serial_number])
    return cmd_executer.exec_commands(cmd)


def from_file(file_path):
    systrace_path = systrace_env.get_executable_systrace()
    if systrace_path is None:
        logger.error("can't find systrace in system environment.")
        sys.exit(1)
    cmd = [systrace_path]
    cmd.extend(["--from-file", file_path])
    return cmd_executer.exec_commands(cmd)


def _drop_cache():
    """
    free pagecache dentries and inodes cache, only root device effected
    """
    (out, return_code) = cmd_executer.exec_write_value("/proc/sys/vm/drop_caches", "3")
    if return_code is 0 and out is None:
        logger.debug("succeed to drop caches.")


def __open_android_fs(serial_number):
    """
    tracing android_fs events,  only root device effected.
    """
    (out, return_code) = cmd_executer.exec_write_value("/d/tracing/events/android_fs/enable", "1")
    open_successful = False
    if return_code is 0 and out is None:
        open_successful = True
        logger.info("succeed to tracing android_fs events.")
    else:
        (out_1, return_code_1) = cmd_executer.exec_adb_shell_with_append_commands(
            "su -c 'echo 1 > /d/tracing/events/android_fs/enable'",
            serial_number)
        if return_code_1 is 0 and out_1 is None:
            open_successful = True
            logger.info("ensure to tracing android_fs events successfully.")
    return open_successful


def __capture_systrace(context):
    systrace_path = systrace_env.get_executable_systrace()
    cmd = ["python2.7", systrace_path]
    if context.serial_number is not None:
        cmd.extend(["-e", context.serial_number])
    if context.categories:
        cmd.extend(context.categories)
    cmd.extend(["-o", context.get_build_file_path(rhea_config.ORIGIN_SYSTRACE_FILE)])
    if context.app_name is not None:
        cmd.extend(["-a", context.app_name])
    if context.trace_time is not None:
        cmd.extend(["-t", str(context.trace_time + context.advanced_systrace_time + 2)])
    if context.trace_buf_size is not None:
        cmd.extend(["-b", str(context.trace_buf_size)])
    if context.kfuncs is not None:
        cmd.extend(["-k", str(context.kfuncs)])
    logger.debug("systrace cmd: " + str(cmd))
    return cmd_executer.exec_commands(cmd)
