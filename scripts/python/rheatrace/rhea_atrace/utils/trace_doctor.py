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

import re
import collections
from decimal import *

from rhea_atrace.utils.file_utils import print_progress
from rhea_atrace.rhea_log.rhea_logger import rhea_logger

logger = rhea_logger


def repair_trace_lines(trace_lines, trace_lines_name):
    time_pattern = re.compile('[0-9]{1,10}[\.][0-9]{6}')
    base_time = 0
    trace_dict = collections.OrderedDict()
    index = 0
    getcontext().prec = 32
    length = len(trace_lines)
    for line in trace_lines:
        time_trace_line = time_pattern.search(line)
        if time_trace_line is not None and ('kernel time now' not in line):
            base_time = float(time_trace_line.group(0))
        else:
            base_time = base_time
        trace_dict[(base_time, index)] = line
        index = index + 1
        # print_progress(index, length, prefix='Sort merged multi-threaded data:', suffix='Complete', bar_length=80)
    logger.debug("Sort %s's trace line by trace time, it maybe takes a long time ... ...", trace_lines_name)
    trace_dict = collections.OrderedDict(sorted(trace_dict.items(), key=lambda t: t[0]))
    logger.debug("Sort %s's trace line completed!", trace_lines_name)
    new_lines = list(trace_dict.values())
    return new_lines


def repair_rhea_lines(rhea_lines):
    real_rhea = []
    stack_dict = collections.OrderedDict()
    index = 0
    length = len(rhea_lines)
    for line in rhea_lines:
        try:
            if len(line.split(' ')) < 3:
                thread_name = 'main'
            else:
                thread_name = line.split(' ')[1].strip().replace(":", "")
            if '|B:' in line:
                method_name = line.split('|B:')[-1].strip()
                line = line.replace('|B:', '|')
                if thread_name not in stack_dict.keys():
                    stack_dict[thread_name] = []
                stack_dict[thread_name].append((thread_name, method_name, 'B', line))
            elif '|E:' in line:
                method_name = line.split('|E:')[-1].strip()
                line = line.replace('|E:', '|').replace('B|', 'E|')
                if thread_name not in stack_dict.keys():
                    stack_dict[thread_name] = []
                stack_dict[thread_name].append((thread_name, method_name, 'E', line))
            else:
                real_rhea.append(line)
        except Exception as e:
            logger.info("repair_rhea_lines error: %s", str(e))
        index = index + 1
        print_progress(index, length, prefix='Processing raw data:', suffix='Complete', bar_length=80)
    value_stack_conut = 0
    index = 0
    for thread_name in stack_dict.keys():
        value_stack = stack_dict[thread_name]
        real_rhea_deep = []
        value_stack_conut = value_stack_conut + len(value_stack)
        while len(value_stack) > 0:
            pop_value = value_stack.pop()
            if pop_value[2] == 'E':
                while len(value_stack) > 0 and value_stack[-1][2] == 'B' and value_stack[-1][1] != pop_value[1]:
                    tmp_line_b = value_stack.pop()
                    tmp_line_e = tmp_line_b
                    time_pattern = re.compile('[0-9]{1,10}[\.][0-9]{6}')
                    pop_value_end_time = time_pattern.search(pop_value[3]).group(0)
                    tmp_line_e_end_time = time_pattern.search(tmp_line_e[3]).group(0)
                    real_rhea.append(tmp_line_b[3].replace("\n", "") + ": CrashTag\n")
                    real_rhea.append(tmp_line_e[3].replace(tmp_line_e_end_time, pop_value_end_time)
                                     .replace(': B', ': E')
                                     .replace("\n", "") + ": CrashTag\n")
                if len(value_stack) > 0 and value_stack[-1][2] == 'B' and value_stack[-1][1] == pop_value[1]:
                    real_rhea.append(value_stack.pop()[3])
                    real_rhea.append(pop_value[3])
                    continue
                if len(value_stack) > 0 and value_stack[-1][2] == 'E':
                    real_rhea_deep.append(pop_value)
                    real_rhea_deep.append(value_stack.pop())
                    continue
            elif pop_value[2] == 'B':
                if len(real_rhea_deep) > 0 and real_rhea_deep[-1][2] == 'E' and real_rhea_deep[-1][1] == pop_value[1]:
                    # Fix the problem of disorder caused by the same timestamp
                    real_rhea.append(pop_value[3])
                    real_rhea.append(real_rhea_deep.pop()[3])
                    continue
                else:
                    real_rhea.append(pop_value[3].replace("\n", "") + ": CrashTag\n")
                    real_rhea.append(pop_value[3].replace(': B', ': E')
                                     .replace("\n", "") + ": CrashTag\n")
        index = index + 1
        # print_progress(index, value_stack_conut, prefix='Repair lost Crash data:', suffix='Complete', bar_length=80)
    real_rhea = repair_trace_lines(real_rhea, "base trace file")
    return real_rhea


def repair_rhea_lines_nosort(rhea_lines):
    real_rhea = []
    stack_dict = collections.OrderedDict()
    index = 0
    length = len(rhea_lines)
    for line in rhea_lines:
        try:
            if len(line.split(' ')) < 3:
                thread_name = 'main'
            else:
                thread_name = line.split(' ')[1].strip().replace(":", "")
            if '|B:' in line:
                method_name = line.split('|B:')[-1].strip()
                line = line.replace('|B:', '|')
                if thread_name not in stack_dict.keys():
                    stack_dict[thread_name] = []
                stack_dict[thread_name].append((thread_name, method_name, 'B', line))
            elif '|E:' in line:
                method_name = line.split('|E:')[-1].strip()
                line = line.replace('|E:', '|').replace('B|', 'E|')
                if thread_name not in stack_dict.keys():
                    stack_dict[thread_name] = []
                stack_dict[thread_name].append((thread_name, method_name, 'E', line))
            # 360.554760 12086: B|11992|T:ApkSigningBlockUtils:findApkSigningBlock
            elif '|T:' in line:
                method_name = line.split('|T:')[-1].strip()
                line = line.replace('|E:', '|').replace('B|', 'E|')
                if thread_name not in stack_dict.keys():
                    stack_dict[thread_name] = []
                stack_dict[thread_name].append((thread_name, method_name, 'T', line))
            else:
                real_rhea.append(line)
        except Exception as e:
            logger.info("repair_rhea_lines error: %s", str(e))
        index = index + 1
        print_progress(index, length, prefix='Processing raw data:', suffix='Complete', bar_length=80)
    value_stack_conut = 0
    index = 0
    for thread_name in stack_dict.keys():
        value_stack = stack_dict[thread_name]
        real_rhea_deep = []
        value_stack_conut = value_stack_conut + len(value_stack)
        while len(value_stack) > 0:
            pop_value = value_stack.pop()
            if pop_value[2] == 'E':
                while len(value_stack) > 0 and value_stack[-1][2] == 'B' and value_stack[-1][1] != pop_value[1]:
                    tmp_line_b = value_stack.pop()
                    tmp_line_e = tmp_line_b
                    time_pattern = re.compile('[0-9]{1,10}[\.][0-9]{6}')
                    pop_value_end_time = time_pattern.search(pop_value[3]).group(0)
                    tmp_line_e_end_time = time_pattern.search(tmp_line_e[3]).group(0)
                    real_rhea.append(tmp_line_b[3].replace("\n", "") + ": CrashTag\n")
                    real_rhea.append(tmp_line_e[3].replace(tmp_line_e_end_time, pop_value_end_time)
                                     .replace(': B', ': E')
                                     .replace("\n", "") + ": CrashTag\n")
                while len(value_stack) > 0 and value_stack[-1][2] == 'T' and value_stack[-1][1] == pop_value[1]:
                    value_stack.pop()
                if len(real_rhea_deep) > 0 and real_rhea_deep[-1][1] == pop_value[1]:
                                    real_rhea_deep.append(pop_value)
                                    continue
                if len(value_stack) > 0 and value_stack[-1][2] == 'T' and value_stack[-1][1] != pop_value[1]:
                    real_rhea_deep.append(pop_value)
                    real_pop = value_stack.pop()
                    throw_end = (real_pop[0], real_pop[1], "E", real_pop[3])
                    real_rhea_deep.append(throw_end)
                if len(value_stack) > 0 and value_stack[-1][2] == 'E':
                    real_rhea_deep.append(pop_value)
                    real_rhea_deep.append(value_stack.pop())
                    continue
                if len(value_stack) > 0 and value_stack[-1][2] == 'B' and value_stack[-1][1] == pop_value[1]:
                    real_rhea.append(value_stack.pop()[3])
                    real_rhea.append(pop_value[3])
                    continue
            elif pop_value[2] == 'B':
                has_match_end, real_rhea_deep, deep_pop = search_end_trace(real_rhea_deep, pop_value[1])
                if has_match_end:
                    real_rhea.append(pop_value[3])
                    real_rhea.append(deep_pop[3])
                else:
                    real_rhea.append(pop_value[3].replace("\n", "") + ": CrashTag\n")
                    real_rhea.append(pop_value[3].replace(': B', ': E')
                                     .replace("\n", "") + ": CrashTag\n")
                    logger.debug(
                        "pop_value is B, value_stack is null or not match pop_value: %s, real_rhea_deep[-1]: %s",
                        pop_value, real_rhea_deep)
        index = index + 1
        print_progress(index, len(stack_dict.keys()), prefix='Repair lost Crash data:', suffix='Complete',
                       bar_length=80)
    return real_rhea


def search_end_trace(real_rhea_deep_list, method):
    for index in range(len(real_rhea_deep_list) - 1, -1, -1):
        tmp = real_rhea_deep_list
        pop = real_rhea_deep_list[index]
        if pop[1] == method:
            while index - 1 > 0 and real_rhea_deep_list[index - 1][1] == method:
                pop = real_rhea_deep_list[index - 1]
                tmp.pop(index)
                index = index - 1
            tmp.pop(index)
            return True, tmp, pop
    return False, real_rhea_deep_list, None


def repair_origin_file(origin_file):
    new_lines = []
    with open(origin_file, 'rb+') as origin_trace:
        origin_trace_lines = origin_trace.readlines()
        for origin_trace_line in origin_trace_lines:
            if 'tracing_mark_write:' in origin_trace_line:
                if ('|E:' in origin_trace_line) or ('|B:' in origin_trace_line):
                    continue
            new_lines.append(origin_trace_line)
    with open(origin_file, 'w') as for_write:
        for new_line in new_lines:
            for_write.write(new_line)
