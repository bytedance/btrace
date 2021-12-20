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


import rhea_config
from common import cmd_executer
from rhea_atrace.rhea_log.rhea_logger import rhea_logger

logger = rhea_logger


def start_tracing(app_name, serial_number):
    cmd = ["shell", "am", "broadcast", "-a", rhea_config.RHEA_TRACE_ACTION_SWITCH_START, app_name]
    (output, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(cmd, serial_number))
    if return_code is 0:
        return True
    else:
        return False


def stop_tracing(app_name, serial_number):
    cmd = ["shell", "am", "broadcast", "-a", rhea_config.RHEA_TRACE_ACTION_SWITCH_STOP, app_name]
    (output, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(cmd, serial_number))
    if return_code is 0:
        return True
    else:
        return False
