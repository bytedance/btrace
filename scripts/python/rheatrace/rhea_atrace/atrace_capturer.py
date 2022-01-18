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

import time
import trace_runner
from common import cmd_executer
from rhea_atrace.rhea_log.rhea_logger import rhea_logger

logger = rhea_logger


def capture(context):
    # check android os version
    check_sdk_version_cmd = ["shell", "getprop", "ro.build.version.sdk"]
    (out, return_code) = cmd_executer.exec_commands(
        cmd_executer.get_complete_abd_cmd(check_sdk_version_cmd, context.serial_number))
    os_version = 0
    if return_code is 0:
        os_version = int(out.replace("\n", ""))
    if os_version >= 30:
        logger.info("try to grant permission MANAGE_EXTERNAL_STORAGE for %s", context.app_name)
        manage_external_storage_cmd = ["shell", "appops", "set", "--uid", context.app_name,
                                       "MANAGE_EXTERNAL_STORAGE", "allow"]
        (out, return_code) = cmd_executer.exec_commands(
            cmd_executer.get_complete_abd_cmd(manage_external_storage_cmd, context.serial_number))
        if return_code is not 0:
            logger.warning("failed to allow MANAGE_EXTERNAL_STORAGE permission for " + context.app_name)
    """" check whether restart or not"""
    if context.restart:
        logger.info("try to force stop " + context.app_name)
        _force_stop_app(context.app_name, context.serial_number)
        logger.info("start to launch app %s", context.app_name)
        start_success = _start_app(context.app_name, context.serial_number)
        if not start_success:
            logger.info("failed to launch app '%s'", context.app_name)
            return False
    else:
        started = _check_started(context.app_name, context.serial_number)
        if not started:
            logger.info("app '%s' is not started, just launch it firstly.", context.app_name)
            start_success = _start_app(context.app_name, context.serial_number)
            if not start_success:
                logger.info("failed to launch app '%s'", context.app_name)
                return False
        else:
            logger.info("app '%s' has been started, just start tracing immediately.", context.app_name)
            trace_runner.start_tracing(context.app_name, context.serial_number)
    wait_time = context.trace_time
    logger.info("start to capture atrace for %d seconds, now you can operate your app.", context.trace_time)
    if wait_time > 0:
        time.sleep(wait_time)
    else:
        return False
    logger.info("stop to capture atrace, it maybe take a long time...")
    trace_runner.stop_tracing(context.app_name, context.serial_number)


def _check_started(app_name, serial_number):
    cmd = ["shell", "ps", "|", "grep", app_name]
    (output, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(cmd, serial_number))
    if return_code is 0 and output is not None:
        process_infos = output.split("\n")
        for process_info in process_infos:
            if process_info.endswith(app_name):
                return True
    else:
        return False


def _start_app(app_name, serial_number):
    launch_activity = __get_launch_activity(app_name, serial_number)
    if launch_activity is None:
        return False
    logger.debug("launch-activity is '%s'", launch_activity)
    cmd = ["shell", "am", "start", "-n", launch_activity]
    (output, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(cmd, serial_number))
    if return_code is 0:
        return True
    else:
        return False


def _force_stop_app(app_name, serial_number):
    cmd = ["shell", "am", "force-stop", app_name]
    (output, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(cmd, serial_number))
    if return_code is 0:
        return True
    else:
        return False


def __get_launch_activity(app_name, serial_number):
    cmd = ["shell", "dumpsys", "package", app_name]
    (output, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(cmd, serial_number))
    if return_code is 0 and output is not None:
        out_lines = output.rstrip().splitlines()
        for index in range(0, len(out_lines)):
            line = out_lines[index]
            if line is not None and line.__contains__("android.intent.action.MAIN:"):
                if __find_launch_category(index, out_lines) is False:
                    continue
                main_activity_line = out_lines[index + 1]
                for sub_line in main_activity_line.split(" "):
                    if sub_line is not "" and sub_line.__contains__(app_name):
                        return sub_line
    return None


def __find_launch_category(index, out_lines):
    count = 2
    while True:
        action_or_category = str(out_lines[index + count]).strip()
        if action_or_category == '':
            break
        if action_or_category.startswith("Action:"):
            count = count + 1
            continue
        if action_or_category.startswith("Category:"):
            count = count + 1
            if action_or_category.__contains__("android.intent.category.LAUNCHER"):
                return True
        else:
            logger.warning("can't find launcher category.")
            return True
    return True
