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

def extend(input_trace, output_trace):
    """form input_trace file get io event, convert stand systrace info write to output_trace file """
    read_dict = {}
    write_dict = {}
    with open(output_trace, "w+") as fw:
        with open(input_trace) as fr:
            for line in fr.readlines():
                if "android_fs_dataread_start" in line:
                    on_dataread_start(line, read_dict, fw)
                    continue
                if "android_fs_dataread_end" in line:
                    on_dataread_end(line, read_dict, fw)
                    continue

                if "android_fs_datawrite_start" in line:
                    on_datawrite_start(line, write_dict, fw)
                    continue

                if "android_fs_datawrite_end" in line:
                    on_datawrite_end(line, write_dict, fw)
                    continue
                fw.write(line)


def on_dataread_start(line, read_dict, fw):
    line = line.replace('\n', '')
    parts = line.partition(": android_fs_dataread_start:")
    proc_info = (parts[0].split('['))[0]
    if parts[2] is '' or None:
        return
    file_info = parts[2].split(',')
    filename = offset = size = pid = ino = ''
    try:
        filename = file_info[0].lstrip('entry_name ')
        offset = file_info[1].lstrip('offset ')
        size = file_info[2].lstrip('bytes ')
        pid = file_info[4].lstrip('pid ')
        ino = file_info[6].lstrip('ino ').strip('\n')
    except:
        pass
    key = ino + ":" + offset
    read_dict[key] = proc_info

    new_line = parts[0] + ": tracing_mark_write: B|" + pid + '|read: ' \
               + filename + ",offset: " + offset + ", size: " + size + '\n'

    fw.write(new_line)


def on_dataread_end(line, read_dict, fw):
    line = line.replace('\n', '')
    parts = line.partition(': android_fs_dataread_end:')
    if parts[2] is '' or None:
        return
    file_info = parts[2].split(',')
    ino = file_info[0].lstrip('ino ')
    offset = file_info[1].lstrip('offset ')
    key = ino + ":" + offset

    if read_dict.__contains__(key):
        proc_info = read_dict[key]
        if not str(proc_info).__contains__("-"):
            return
        pid = proc_info.split('-')[1].split(' ')[0]
        timestamp = "[" + (parts[0].split('['))[1]
        new_line = proc_info + timestamp + ": tracing_mark_write: E|" + pid + "\n"
        fw.write(new_line)
        del read_dict[key]


def on_datawrite_start(line, write_dict, fw):
    parts = line.partition(": android_fs_datawrite_start:")
    proc_info = (parts[0].split('['))[0]
    if parts[2] is '' or None:
        return
    file_info = parts[2].split(',')
    filename = offset = size = pid = ino = ''
    try:
        filename = file_info[0].lstrip('entry_name ')
        offset = file_info[1].lstrip('offset ')
        size = file_info[2].lstrip('bytes ')
        pid = file_info[4].lstrip('pid ')
        ino = file_info[6].lstrip('ino ').strip('\n')
    except:
        pass
    key = ino + ":" + offset
    write_dict[key] = proc_info
    new_line = parts[0] + ": tracing_mark_write: B|" + pid + '|write: ' + filename + \
               ", offset: " + offset + ", size: " + size + '\n'
    fw.write(new_line)


def on_datawrite_end(line, write_dict, fw):
    parts = line.partition(': android_fs_datawrite_end:')
    if parts[2] is '' or None:
        return
    file_info = parts[2].split(',')
    ino = file_info[0].lstrip('ino ')
    offset = file_info[1].lstrip('offset ')

    key = ino + ":" + offset
    if write_dict.__contains__(key):
        proc_info = write_dict[key]
        if not str(proc_info).__contains__("-"):
            return
        pid = proc_info.split('-')[1].split(' ')[0]
        timestamp = "[" + (parts[0].split('['))[1]

        new_line = proc_info + timestamp + ": tracing_mark_write: E|" + pid + "\n"
        fw.write(new_line)

        del write_dict[key]
