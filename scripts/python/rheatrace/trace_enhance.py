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
from rhea_atrace.utils.file_utils import *
from rhea_atrace.utils.trace_doctor import *
from decimal import *
import json

logger = rhea_logger

DEBUG = False


def map_get(map, key, def_value):
    if key in map.keys():
        return map[key]
    return def_value


class LockAction:
    """ a single lock action like lock/unlock/await/signal """

    def __init__(self, format):
        # multi-thread
        # 4451304.157739 19786: B|19526|ReentrantLock[147755888]:locked
        # if mainThreadOnly
        # 601868.050568: B|3920|ReentrantLock[260395896]:lock
        time_tid = format.split(":")[0].split(" ")
        self.time = Decimal(time_tid[0])
        self.tid = 0 if len(time_tid) == 1 else time_tid[1]
        self.lock = format.split("[")[1].split("]")[0]
        self.func = format.split(":")[-1][:-1]


class LockPeriod:
    """ a lock <-> unlock period in single thread. a thread may have many LockPeriods """

    def __init__(self, lock, tid, lock_time, locked_time, unlock_time, unlocked_time):
        self.lock = lock
        self.tid = tid
        self.lock_time = lock_time
        self.locked_time = locked_time
        self.unlock_time = unlock_time
        self.unlocked_time = unlocked_time
        self.contention = None
        self.contention_time = 0
        self.waiters = 0

    def __str__(self):
        return "lock:{}, tid:{}, contention:{}, time: [lock:{}, locked:{}, unlocked:{}]" \
            .format(self.lock, self.tid, self.contention, self.lock_time, self.locked_time, self.unlocked_time)


class FutureInfo:
    def __init__(self, format):
        # multi-thread
        # 4451304.157739 19786: B|19526|Future[147755888]:exe
        # if mainThreadOnly
        # 601868.050568: B|3920|Future[260395896]:get
        time_tid = format.split(":")[0].split(" ")
        self.time = Decimal(time_tid[0])
        self.tid = 0 if len(time_tid) == 1 else time_tid[1]
        self.id = format.split("[")[1].split("]")[0]
        self.type = format.split(":")[-1][:-1]


def group_lock_actions_to_thread_lock_action_list(lock_actions):
    group_by_tid = {}
    for action in lock_actions:
        if action.tid not in group_by_tid:
            group_by_tid[action.tid] = ThreadLockActionList(action.tid)
        group_by_tid[action.tid].add_lock_action(action)
    return group_by_tid.values()


def make_period_list(lock_actions):
    belong_lock = lock_actions[0].lock
    belong_thread = lock_actions[0].tid
    for action in lock_actions:
        if action.lock != belong_lock or action.tid != belong_thread:
            print("make period error. unexpected LockActions")
            sys.exit()

    result = []
    lock_actions.sort(key=lambda s: s.time)
    stack = []
    for action in lock_actions:
        if action.func in ["lock", "tryLock", "lockInterruptibly", "locked"]:
            stack.append(action)
        elif action.func == "unlocked":
            """ find the matching locked """
            while len(stack) > 0:
                locked = stack.pop()
                if locked.func == "locked":
                    lock_time = locked.time
                    if len(stack) > 0:
                        temp = stack.pop()  # find the lock
                        if temp.func in ["lock", "tryLock", "lockInterruptibly"]:
                            lock_time = temp.time
                        else:
                            stack.append(temp)
                    result.append(LockPeriod(
                        belong_lock, belong_thread, lock_time, locked.time, action.time, action.time))
                    break
    return result


class ThreadLockActionList:
    """ all lock actions in single thread """

    def __init__(self, thread):
        self.thread = thread
        self.actions = []
        self.group_by_lock = {}
        self.lock_periods = []

    def add_lock_action(self, lock_action):
        self.actions.append(lock_action)
        if lock_action.lock not in self.group_by_lock:
            self.group_by_lock[lock_action.lock] = []
        self.group_by_lock[lock_action.lock].append(lock_action)

    def make_period(self):
        for lock in self.group_by_lock:
            periods = make_period_list(self.group_by_lock[lock])
            self.lock_periods.extend(periods)
        return self.lock_periods


def mark_contention_and_waiters(all_lock_periods):
    """
    thread 1 -> ------ lock ------ locked ------ unlocked ------
    thread 2 -> ------------- lock ---------------------- locked
    thread 3 -> lock ------------------------------------ ...... locked 
    => contention means A thread <locked> after B thread <unlocked> and A's <lock> happens before B's <unlocked>
    """
    for p in all_lock_periods:
        for pp in all_lock_periods:
            if p.tid == pp.tid or p.lock != pp.lock:
                continue
            if pp.lock_time <= p.unlocked_time <= pp.locked_time:
                if p.unlocked_time < pp.contention_time or pp.contention_time == 0:
                    pp.contention = p.tid
                    pp.contention_time = p.unlocked_time
    """ contention tid appears times means waiters"""
    contention_counter = {}
    for p in all_lock_periods:
        if p.contention is not None:
            if p.contention not in contention_counter:
                contention_counter[p.contention] = 0
            contention_counter[p.contention] += 1
    for p in all_lock_periods:
        if p.contention is not None:
            p.waiters = contention_counter[p.contention]


def find_in_lock_period(periods, action):
    for p in periods:
        if p.tid == action.tid and p.lock == action.lock and p.lock_time == action.time:
            return p
    return None


class TraceEnhance:
    def __init__(self, processor):
        self.trace_processor = processor
        self.native_binder_info = None
        self.tid_tname = processor.tid_and_task_name_dict

    def enhance(self):
        pro = self.trace_processor
        self.enhance_reentrant_lock()
        self.enhance_binder_context()
        self.enhance_future()
        self.enhance_thread_creation()
        """ clear all <rhea-draft> """
        for idx in range(len(pro.rhea_origin_lines)):
            pro.rhea_origin_lines[idx] = pro.rhea_origin_lines[idx].replace("<rhea-draft>", "")

    def get_tname_by_tid(self, tid, default_tname):
        tid = str(tid)
        if tid in self.tid_tname.keys():
            return self.tid_tname[tid]
        return default_tname

    def enhance_reentrant_lock(self):
        pro = self.trace_processor
        """ read all ReentrantLock related sections """
        all_lock_actions = []
        for line in pro.rhea_origin_lines:
            if "<rhea-draft>ReentrantLock[" in line:
                all_lock_actions.append(LockAction(line))

        all_lock_periods = []
        thread_lock_action_lists = group_lock_actions_to_thread_lock_action_list(all_lock_actions)
        for thread_action_list in thread_lock_action_lists:
            periods = thread_action_list.make_period()
            all_lock_periods.extend(periods)
        mark_contention_and_waiters(all_lock_periods)
        """ enhance trace section """
        for idx in range(len(pro.rhea_origin_lines)):
            line = pro.rhea_origin_lines[idx]
            if "<rhea-draft>ReentrantLock[" in line:
                action = LockAction(line)
                if action.func in ["lock", "lockInterruptibly"]:  # tryLock no contention.
                    period = find_in_lock_period(all_lock_periods, action)
                    if period is not None and period.contention is not None:
                        contention_name = self.get_tname_by_tid(period.contention, "?")
                        suffix = " contention with {}({}). waiters={}\n" \
                            .format(contention_name, period.contention, period.waiters)
                        pro.rhea_origin_lines[idx] = line[:-1] + suffix
                """ reformat output"""
                new_line = pro.rhea_origin_lines[idx].replace("ReentrantLock[" + action.lock + "]", "ReentrantLock")
                pro.rhea_origin_lines[idx] = new_line[:-1] + ". id=" + action.lock + "\n"

    def enhance_binder_context(self):
        pro = self.trace_processor
        """ read pre-defined native binders info"""
        current_path = os.path.abspath(os.path.dirname(__file__))
        native_binder_mapping_path = os.path.join(current_path, "config/native_binder_code_mapping.json")
        with open(native_binder_mapping_path) as json_file:
            self.native_binder_info = json.load(json_file)
        """ parse binder.txt to map"""
        binder_code_map = {}
        lines = pro.pull_read_rhea_binder_file()
        if len(lines) == 0:
            print("binder info is empty")
            return
        current_token = ""
        for line in lines:
            if line[0] == '#':
                current_token = line[1:-1]
                binder_code_map[current_token] = {}
            else:
                pair = line.split(":")
                code = int(pair[1])
                binder_code_map[current_token][code] = pair[0]
        """ map binder name and code to specific method"""
        for idx in range(len(pro.rhea_origin_lines)):
            line = pro.rhea_origin_lines[idx]
            if "|<rhea-draft>binder transact[" in line:
                label = line.split("[")[1]
                if ":" in label:
                    comps = label.split(":")
                    token = comps[0]
                    code = int(comps[1].split("]")[0])
                    # assign token and code
                    code_name = None
                    if token in binder_code_map.keys() and code in binder_code_map[token].keys():
                        code_name = binder_code_map[token][code]
                        if code_name.startswith("TRANSACTION_"):
                            code_name = code_name[12:]
                    elif token in self.native_binder_info.keys() and code < len(self.native_binder_info[token]):
                        code_name = self.native_binder_info[token][code]
                    elif DEBUG:
                        print("binder-enhance: unknown bind " + token + ":" + str(code))
                    if code_name is not None:
                        new_line = line.replace(":" + str(code) + "]", ":" + code_name + "|" + str(code) + "]")
                        pro.rhea_origin_lines[idx] = new_line

    def enhance_future(self):
        pro = self.trace_processor
        future_exed_map = {}
        for line in pro.rhea_origin_lines:
            if "<rhea-draft>Future[" in line:
                info = FutureInfo(line)
                if info.type == "done":
                    future_exed_map[info.id] = info
        for idx in range(len(pro.rhea_origin_lines)):
            line = pro.rhea_origin_lines[idx]
            if "<rhea-draft>Future[" in line:
                info = FutureInfo(line)
                if info.type == "get":
                    if info.id in future_exed_map.keys():
                        exed = future_exed_map[info.id]
                        if exed.time > info.time:  # Future:get before task ends
                            tname = self.get_tname_by_tid(exed.tid, "?")
                            line = line.replace("[{}]".format(str(info.id)), "")
                            pro.rhea_origin_lines[idx] = "{} wait on {}({}). id={}\n" \
                                .format(line[:-1], tname, str(exed.tid), str(info.id))
                            continue
                line = line.replace("[{}]".format(str(info.id)), "")
                pro.rhea_origin_lines[idx] = line[:-1] + ". id={}\n".format(str(info.id))
        pass

    def enhance_thread_creation(self):
        pro = self.trace_processor
        fake_tid_parent_map = {}
        fake_tid_child_map = {}
        for line in pro.rhea_origin_lines:
            if "<rhea-draft>pthread_" in line:
                fake_tid = int(line.split("tid=")[1])
                time_tid = line.split(":")[0].split(" ")
                if len(time_tid) == 2:
                    if "pthread_create " in line:
                        fake_tid_parent_map[fake_tid] = int(time_tid[1])
                    else:
                        fake_tid_child_map[fake_tid] = int(time_tid[1])
        for idx in range(len(pro.rhea_origin_lines)):
            line = pro.rhea_origin_lines[idx]
            if "<rhea-draft>pthread_" in line:
                fake_tid = int(line.split("tid=")[1])
                parent_tid = map_get(fake_tid_parent_map, fake_tid, -1)
                child_tid = map_get(fake_tid_child_map, fake_tid, -1)
                if "pthread_create " in line:
                    line = line.replace(" tid=" + str(fake_tid), " child_tid=" + str(child_tid)
                                        + "(" + self.get_tname_by_tid(child_tid, "?") + ")")
                else:
                    line = line.replace(" tid=" + str(fake_tid), " parent_tid=" + str(parent_tid)
                                        + "(" + self.get_tname_by_tid(parent_tid, "?") + ")")
                pro.rhea_origin_lines[idx] = line
        pass
