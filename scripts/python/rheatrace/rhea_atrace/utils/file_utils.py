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

from common import cmd_executer


def get_last_line(base_file):
    file_size = os.path.getsize(base_file)
    block_size = 1024
    dat_file = open(base_file, 'r')
    last_line = ""
    if file_size > block_size:
        max_seek_point = (file_size // block_size)
        dat_file.seek((max_seek_point - 1) * block_size)
    elif file_size:
        dat_file.seek(0, 0)
    lines = dat_file.readlines()
    if lines:
        last_line = lines[-1].strip()
    dat_file.close()
    return last_line


def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        pass
    try:
        import unicodedata
        unicodedata.numeric(s)
        return True
    except (TypeError, ValueError):
        pass
    return False


def append_space_from_left(name, length):
    loop_count = length - len(name)
    prefix = ""
    for index in range(loop_count):
        prefix = prefix + " "
    return prefix + name


def append_space_from_right(name, length):
    loop_count = length - len(name)
    suffix = ""
    for index in range(loop_count):
        suffix = suffix + " "
    return name + suffix


def del_android_device_file(serial_number, path, del_file):
    commands = [
        "cd " + path,
        "rm -f {}".format(del_file),
        "exit"
    ]
    return cmd_executer.exec_adb_shell_with_append_commands(commands, serial_number)


def print_progress(iteration, total, prefix='', suffix='', decimals=1, bar_length=100):
    format_str = "{0:." + str(decimals) + "f}"
    percent = format_str.format(100 * (iteration / float(total)))
    sys.stdout.write('\r%s %s%s %s' % (prefix, percent, '%', suffix)),
    if iteration == total:
        sys.stdout.write('\n')
    sys.stdout.flush()