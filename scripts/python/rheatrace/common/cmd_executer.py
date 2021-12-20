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

import subprocess
import context


def exec_commands(cmd):
    """Echo and run the given command.

    Args:
      cmd: the command represented as a list of strings.
    Returns:
      A tuple of the output and the exit code.
    """
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output, error = process.communicate()
    return (output, process.returncode)


# can exec_adb_shell_cmd
def exec_adb_shell_with_append_commands(cmds, serial_number):
    """run the given commandList.

    Args:
      cmds: list of string commands.
            like this: cmds = ["cd sdcard/rhea-trace","rm -r AAAAAAAAAAB/","exit",]
      serial_number: serial number of devices
    """
    process = subprocess.Popen("adb -s " + serial_number + " shell", shell=True, stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    output, error = process.communicate("\n".join(cmds) + "\n")
    return (output, process.returncode)


def exec_write_value(device_path, contents):
    from devil.android import device_utils
    from devil.android import device_errors
    adb_return_code = 0
    device_serial = context.Context.instance().serial_number
    device = device_utils.DeviceUtils.HealthyDevices(device_arg=device_serial)[0]
    try:
        adb_output = device.WriteFile(device_path, contents)
    except device_errors.AdbShellCommandFailedError as error:
        adb_return_code = error.status
        adb_output = error.output
    return (adb_output, adb_return_code)


def get_complete_abd_cmd(tail_cmds, serial_number):
    cmd = ["adb"]
    if serial_number is not None:
        cmd.extend(["-s", serial_number])
    cmd.extend(tail_cmds)
    return cmd
