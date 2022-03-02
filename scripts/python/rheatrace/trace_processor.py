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
import multiprocessing
import re
import time

import rhea_config
from enhanced_systrace import systrace_env
from rhea_atrace.utils.file_utils import *
from rhea_atrace.utils.trace_doctor import *
from decimal import *

from trace_enhance import TraceEnhance

logger = rhea_logger


def parse_key_value_pairs(line):
    """ parse input a=b c=d into map"""
    next_value = True
    last_end = len(line)
    stack = list()
    for idx in reversed(range(len(line))):
        stop = '=' if next_value else ' '
        if line[idx] == stop:
            sub = line[idx + 1: last_end]
            last_end = idx
            next_value = not next_value
            stack.append(sub)
    result = {}
    while len(stack) != 0:
        key = stack.pop()
        value = stack.pop()
        result[key] = value
    return result


class TraceProcessor:
    def __init__(self, context):
        self.serial_number = context.serial_number
        self.package_name = context.app_name

        self.gz_atrace_file = context.get_build_file_path(rhea_config.ATRACE_GZ_FILE)
        self.raw_atrace_file = context.get_build_file_path(rhea_config.ATRACE_RAW_FILE)
        self.atrace_binder_file = context.get_build_file_path(rhea_config.ATRACE_BINDER_FILE)
        systrace_io_used = context.get_params(rhea_config.ENVIRONMENT_PARAMS_ANDROID_FS)
        if systrace_io_used:
            self.systrace_origin_file = context.get_build_file_path(rhea_config.ORIGIN_SYSTRACE_FS_FILE)
        else:
            self.systrace_origin_file = context.get_build_file_path(rhea_config.ORIGIN_SYSTRACE_FILE)
        if not os.path.exists(self.systrace_origin_file):
            logger.error("systrace file '%s' is not existed.", self.systrace_origin_file)
            return

        self.systrace_origin_lines = list()
        self.rhea_origin_lines = list()
        self.tid_and_task_name_dict = ()
        self.systrace_lines = list()
        self.rhea_trace_lines = list()
        self.rhea_standard_lines = list()
        self.binder_origin_lines = list()

        self.rhea_trace_file = context.get_build_file_path(rhea_config.ATRACE_SYS_FILE)
        self.rhea_standard_file = context.get_build_file_path(rhea_config.ATRACE_STANDARDIZED_FILE)
        self.merge_file = context.output_file

        self.monotonic_time = 0
        self.monotonic_difference = 0
        self.systrace_flag = 0
        self.first_systrace_time = Decimal('0.000000')
        self.first_atrace_time = Decimal('0.000000')
        self.is_real_monotonic = False

    def pull_read_rhea_binder_file(self):
        binder_path = rhea_config.ATRACE_APP_GZ_FILE_LOCATION \
                          .replace("%s", self.package_name) + rhea_config.ATRACE_BINDER_FILE
        pull_cmd = ["pull", binder_path, self.atrace_binder_file]
        (out, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(pull_cmd, self.serial_number))
        if return_code is 0:
            with open(self.atrace_binder_file, 'rb') as f:
                self.binder_origin_lines = f.readlines()
        return self.binder_origin_lines

    def unzip_rhea_original_file(self):
        atrace_app_path = rhea_config.ATRACE_APP_GZ_FILE_LOCATION.replace(
            "%s", self.package_name) + rhea_config.ATRACE_GZ_FILE
        pull_cmd = ["pull", atrace_app_path, self.gz_atrace_file]
        (out, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(pull_cmd, self.serial_number))
        if return_code is not 0:
            warm_prompt = "\nWARM PROMPT\n"
            warm_prompt_desc1 = "1. Make sure you have grant permission of writing external storage.\n"
            warm_prompt_desc2 = "2. Make sure RheaTrace has been integrated into your app and inited successfully.\n"
            warm_prompt_desc3 = "3. if `startWhenAppLaunch` is false, you can't capture trace when launch app.\n"
            logger.error(out + warm_prompt + warm_prompt_desc1 + warm_prompt_desc2 + warm_prompt_desc3)
            return False, "unable to pull raw atrace file from device."
        if os.path.getsize(self.gz_atrace_file) is 0:
            # todo::remove empty file.
            return False, "file " + atrace_app_path + " is empty, no atrace wrote."
        # todo:compat for windows.
        gzip_cmd = ["gzip", "-d", "-f", self.gz_atrace_file]
        (out, return_code) = cmd_executer.exec_commands(gzip_cmd)
        if return_code is not 0:
            return False, "failed to unzip file {}".format(self.gz_atrace_file)
        if not os.path.exists(self.raw_atrace_file):
            return False, "file {} is not exist.".format(self.raw_atrace_file)
        last_line = get_last_line(self.raw_atrace_file)
        if not last_line.__contains__("TRACE_END"):
            logger.warning(
                atrace_app_path + " hasn't 'TRACE_END'")
        else:
            (out, return_code) = del_android_device_file(self.serial_number,
                                                         rhea_config.ATRACE_APP_GZ_FILE_LOCATION.replace(
                                                             "%s", self.package_name),
                                                         "rhea-atrace.gz")
            if return_code is not 0:
                logger.warning("failed to delete " + atrace_app_path)
        return True, ""

    def read_task_name_from_systrace(self):
        repair_origin_file(self.systrace_origin_file)
        with open(self.systrace_origin_file, 'rb') as fo:
            self.systrace_origin_lines = fo.readlines()
        find_counts = 0
        self.tid_and_task_name_dict = dict()
        """ 
        parse from lines like
        <...>-32347   (  32347) [007] .... 239854.417866: task_newtask: pid=32390 comm=.sample.android clone_flags=3d0f00 oom_score_adj=0
        <...>-32390   (  32347) [005] ...1 239854.417989: task_rename: pid=32390 oldcomm=.sample.android newcomm=Thread 2 oom_score_adj=0
        <...>-32390   (  32347) [004] .n.1 239854.418157: task_rename: pid=32390 oldcomm=Thread 2 newcomm=lock_test 0 oom_score_adj=0
        """
        for line in self.systrace_origin_lines:
            if "task_newtask: pid=" in line:
                map = parse_key_value_pairs(line)
                if "pid" in map.keys() and "comm" in map.keys():
                    self.tid_and_task_name_dict[map["pid"]] = map["comm"]
            elif "task_rename: pid=" in line:
                map = parse_key_value_pairs(line)
                if "pid" in map.keys() and "newcomm" in map.keys():
                    self.tid_and_task_name_dict[map["pid"]] = map["newcomm"]
        """
        parse from USER PID TID CMD
        """
        for line in self.systrace_origin_lines:
            if find_counts is 2:
                no_shifter_line = line.replace("\n", "")
                if no_shifter_line is not None:
                    section = no_shifter_line.split(" ")
                    result = list()
                    for value in section:
                        if value is not '':
                            result.append(value)
                    if len(result) < 4:
                        break
                    self.tid_and_task_name_dict[result[2]] = result[3]
            if line.startswith("USER"):
                special_key = ' '.join(line.split())
                if special_key == "USER PID TID CMD":
                    find_counts = find_counts + 1

    def _standardize_internal(self):
        logger.debug("start to standardize file %s, please wait.", self.raw_atrace_file)
        start_time = time.time()
        getcontext().prec = 32
        logger.debug("start to unzip file %s, please wait.", self.raw_atrace_file)
        unzip_result, error_msg = self.unzip_rhea_original_file()
        logger.debug("unzip file %s finished %s %s.", self.raw_atrace_file, unzip_result, error_msg)
        if not unzip_result:
            return False, error_msg
        self.systrace_origin_lines = open(self.systrace_origin_file, 'rb').readlines()
        if self.systrace_origin_lines <= 7:
            logger.error("systrace %s write unfinished", self.systrace_origin_file)
            return
        for tmp_index in range(0, len(self.systrace_origin_lines)):
            if "# tracer: nop" in self.systrace_origin_lines[tmp_index]:
                self.systrace_flag = tmp_index + 1
                while self.systrace_origin_lines[self.systrace_flag].startswith('#'):
                    self.systrace_flag = self.systrace_flag + 1
        time_pattern = re.compile('[0-9]{1,10}[\.][0-9]{6}')
        time_trace_line = time_pattern.search(self.systrace_origin_lines[self.systrace_flag])
        if time_trace_line is not None and ('kernel time now' not in self.systrace_origin_lines[self.systrace_flag]):
            self.first_systrace_time = Decimal(time_trace_line.group(0))
        self.rhea_origin_lines = repair_rhea_lines_nosort(open(self.raw_atrace_file, 'rb').readlines())
        pid = None
        self.read_task_name_from_systrace()
        TraceEnhance(self).enhance()
        s_symbol = " [001] ...1 "
        t_symbol = ": tracing_mark_write:"
        index = 0
        length = len(self.rhea_origin_lines)
        for rhea_origin_line in self.rhea_origin_lines:
            if 'TRACE_START' in rhea_origin_line:
                # 512|TRACE_START|1059724457403076|13923|2147483647|2|18316
                start_section = rhea_origin_line.split("|")
                pid = start_section[3]
                index = index + 1
                continue
            if 'TRACE_END' in rhea_origin_line:
                index = index + 1
                continue
            if pid is None:
                raise Exception("Can't find pid in TRACE_START label.")
            section = rhea_origin_line.split(":", 1)
            if len(section) == 2:
                section_left = section[0]
                atrace = section[1]
                if is_number(section_left):
                    main_thread_only = True
                    atrace_time = section_left
                    tid = pid
                else:
                    main_thread_only = False
                    sub_section = section_left.split(" ")
                    if len(sub_section) == 2:
                        sub_section_left = sub_section[0]
                        sub_section_right = sub_section[1]
                        if is_number(sub_section_left):
                            atrace_time = sub_section_left
                            tid = sub_section_right
                        else:
                            logger.warning("unexpected rheatrace file format.:" + rhea_origin_line)
                            continue
                    else:
                        logger.warning("unexpected rheatrace file format.:" + rhea_origin_line)
                        continue
            else:
                logger.warning("trace line broken:" + rhea_origin_line)
                index = index + 1
                continue
            if atrace_time is None or tid is None:
                raise Exception("unexpected rheatrace file format.")
            format_pid = "(" + append_space_from_left(pid, 5) + ")"
            format_tid = append_space_from_right(tid, 5)
            task_name = "<...>"
            if main_thread_only:
                task_name = self.package_name
                task_name_length = len(self.package_name)
                if len(self.package_name) > 15:
                    task_name = self.package_name[task_name_length - 15: task_name_length]
                if task_name.startswith("."):
                    task_name = task_name[1:]
                task_name = append_space_from_left(task_name, 16)
            else:
                if self.tid_and_task_name_dict.__contains__(tid):
                    task_name = self.tid_and_task_name_dict[tid]
                task_name = append_space_from_left(task_name, 16)
            # 948220.911051: B|18557|monotonic_time: 232073.280777
            if 'monotonic_time' in rhea_origin_line:
                self.first_atrace_time = Decimal(atrace_time)
                self.monotonic_time = Decimal(rhea_origin_line.split('monotonic_time: ')[-1])
                self.monotonic_difference = self.monotonic_time - self.first_atrace_time
                if abs(self.first_systrace_time - self.first_atrace_time) > abs(
                        self.first_systrace_time - self.monotonic_time):
                    self.is_real_monotonic = True
                logger.debug("first_atrace_time: %s monotonic_time: %s"
                             " first_systrace_time: %s is_real_monotonic: %s",
                             self.first_atrace_time, self.monotonic_time,
                             self.first_systrace_time, self.is_real_monotonic)
                index = index + 1
                continue
            if self.is_real_monotonic:
                atrace_time = str(Decimal(atrace_time) + Decimal(self.monotonic_difference))
            sys_line = task_name + "-" + format_tid + " " + format_pid + s_symbol + atrace_time + t_symbol + atrace
            standard_line = task_name + "-" + format_tid + s_symbol + atrace_time + t_symbol + atrace
            if sys_line is not None:
                self.rhea_trace_lines.append(sys_line)
            if standard_line is not None:
                self.rhea_standard_lines.append(sys_line)
            index = index + 1
            print_progress(index, length, prefix='Write standard trace file:', suffix='Complete', bar_length=80)
        return True, None

    def write_standard_file(self):
        logger.debug("Write atrace-standard file")
        f_rhea_standard = open(self.rhea_standard_file, 'w')
        f_rhea_standard.write("TRACE:\n# tracer: nop\n")
        lines = repair_trace_lines(self.rhea_standard_lines, "standard trace file")
        for line in lines:
            f_rhea_standard.write(line)
        f_rhea_standard.close()

    def merge_atrace_systrace(self):
        logger.debug("merge atrace and systrace file")
        start_time = time.time()
        """when open merged trace file in chrome, chrome will report an error, so remove this line"""
        slice_err_ = "if(ts<slice.start){throw new Error('Slice '+slice.title+' end time is before its start.');}"
        # merge and repair real trace lines
        if self.systrace_flag == 0:
            logger.error("systrace %s write unfinished, because of missing tracer: nop ",
                         self.systrace_origin_file)
            return
        f_target = open(self.merge_file, 'w')
        for systrace_line in self.systrace_origin_lines[:self.systrace_flag]:
            if slice_err_ in systrace_line:
                systrace_line = systrace_line.replace(slice_err_, "")
            f_target.write(systrace_line)
        real_trace_lines = repair_trace_lines(self.systrace_origin_lines[self.systrace_flag:-6] + self.rhea_trace_lines,
                                              "merger trace file")
        for line in real_trace_lines:
            f_target.write(line)
        for line in self.systrace_origin_lines[-6:]:
            f_target.write(line)
        f_target.close()
        logger.info("congratulation! %s assembles finished, time cost %s s", os.path.abspath(self.merge_file),
                    str(time.time() - start_time))

    def processor(self):
        result, error_msg = self._standardize_internal()
        if result:
            logger.info("write trace data to file, it maybe take a long time...")
            p = multiprocessing.Pool(4)
            aObj = p.apply_async(self, args=('write_standard_file',))
            aObj = p.apply_async(self, args=('merge_atrace_systrace',))
            p.close()
            p.join()
        else:
            logger.info("unzip atrace file: %s, %s", result, error_msg)

    def __call__(self, sign):
        if sign == 'write_standard_file':
            return self.write_standard_file()
        elif sign == 'merge_atrace_systrace':
            return self.merge_atrace_systrace()