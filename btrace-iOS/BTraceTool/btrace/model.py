# Copyright (C) 2025 ByteDance Inc.
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

from typing import Dict, List
from enum import Enum


class SymbolInfo:

    def __init__(self, address: int, name="", image="", file="", is_sys=True):
        self.address = address
        self.name = name
        self.image = image
        self.file = file
        self.is_sys = is_sys

    def is_same(self, symbol):
        # only use symbol name to check if two symbol is identical
        symbol: SymbolInfo = symbol
        if self.name and symbol.name:
            return self.name == symbol.name

        return self.address == symbol.address


class ImageInfo:

    def __init__(self, address: int, path: str, uuid: str):
        self.address = address
        self.path = path
        self.name = path.split("/")[-1]
        self.uuid = uuid
        self.is_sys = ImageInfo.is_sys_image(path)

    @staticmethod
    def is_sys_image(path: str):
        return path.find("/Application/") == -1


def image_info_key(image_info: ImageInfo):
    return image_info.address


class CallstackNode:
    def __init__(self, address: int, parent: int, stack_id: int):
        self.stack_id = stack_id
        self.address = int(address)
        self.parent = int(parent)


class ThreadInfo:
    def __init__(self, tid: int, name: str):
        self.tid = tid
        self.name = name


class PerfettoCounterInfo:
    def __init__(self, type: str, time: int, val: int):
        self.type = type
        self.start_time = time
        self.val = val


class PerfettoSliceInfo:
    def __init__(self, type: str, begin_time: int, end_time: int):
        self.type = type
        self.begin_time = begin_time
        self.end_time = end_time


class AsyncSampleNode:

    def __init__(
        self,
        source_time: int,
        source_cpu_time: int,
        target_tid: int,
        target_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.start_time = source_time
        self.start_cpu_time = source_cpu_time
        self.target_tid = target_tid
        self.target_time = target_time
        self.stack_id = stack_id
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class AsyncSample(AsyncSampleNode):

    def __init__(
        self,
        tid: int,
        start_time: int,
        source_cpu_time: int,
        target_tid: int,
        target_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.type = "async"
        self.tid = tid
        self.start_time = start_time
        self.start_cpu_time = source_cpu_time
        self.target_tid = target_tid
        self.target_time = target_time
        self.stack_id = stack_id
        self.calls: List[int] = []
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class DispatchSampleNode:

    def __init__(
        self,
        source_time: int,
        source_cpu_time: int,
        target_tid: int,
        target_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.start_time = source_time
        self.start_cpu_time = source_cpu_time
        self.target_tid = target_tid
        self.target_time = target_time
        self.stack_id = stack_id
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class DispatchSample(DispatchSampleNode):

    def __init__(
        self,
        tid: int,
        start_time: int,
        source_cpu_time: int,
        target_tid: int,
        target_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.type = "dispatch"
        self.tid = tid
        self.start_time = start_time
        self.start_cpu_time = source_cpu_time
        self.target_tid = target_tid
        self.target_time = target_time
        self.stack_id = stack_id
        self.calls: List[int] = []
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class LockAction(Enum):
    kMtxLock = 1
    kMtxTrylock = 2
    kMtxUnlock = 3
    kCondWait = 4
    kCondTimedwait = 5
    kCondSignal = 6
    kCondBroadcast = 7
    kRdlock = 8
    kTryrdlock = 9
    kWrlock = 10
    kTrywrlock = 11
    kRwUnlock = 12
    kUnfairLock = 13
    kUnfairUnlock = 14
    kMtxLockContention = 15
    kMtxUnlockContention = 16
    kRdlockContention = 17
    kWrlockContention = 18
    kRwUnlockContention = 19
    kUnfairTrylock = 20
    kUnfairLockContention = 21
    kUnfairUnlockContention = 22


class LockSampleNode:

    def __init__(
        self,
        id: int,
        action: int,
        time: int,
        cpu_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.id = id
        self.action = action
        self.start_time = time
        self.start_cpu_time = cpu_time
        self.stack_id = stack_id
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class LockSample:

    def __init__(
        self,
        tid: int,
        id: int,
        action: int,
        time: int,
        cpu_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.type = "lock"
        self.id = id
        self.action = action
        self.start_time = time
        self.start_cpu_time = cpu_time
        self.stack_id = stack_id
        self.tid = tid
        self.calls: List[int] = []
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class DateSample:

    def __init__(
        self,
        tid: int,
        start_time: int,
        cpu_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.type = "date"
        self.tid = tid
        self.start_time = start_time
        self.start_cpu_time = cpu_time
        self.stack_id = stack_id
        self.calls: List[int] = []
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class DateSampleNode:

    def __init__(
        self, time: int, cpu_time: int, stack_id: int, alloc_size: int, alloc_count: int
    ):
        self.start_time = time
        self.start_cpu_time = cpu_time
        self.stack_id = stack_id
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class MemSample:

    def __init__(
        self,
        tid: int,
        size: int,
        start_time: int,
        cpu_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.type = "mem"
        self.tid = tid
        self.size = size
        self.start_time = start_time
        self.start_cpu_time = cpu_time
        self.stack_id = stack_id
        self.calls: List[int] = []
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class MemSampleNode:

    def __init__(
        self,
        addr: int,
        size: int,
        time: int,
        cpu_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.addr = addr
        self.size = size
        self.start_time = time
        self.start_cpu_time = cpu_time
        self.stack_id = stack_id
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class CPUSample:

    def __init__(
        self,
        tid: int,
        start_time: int,
        end_time: int,
        start_cpu_time: int,
        end_cpu_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
        calls: List[int] = None,
    ):
        self.type = "cpu"
        self.tid = tid
        self.start_time = start_time
        self.end_time = end_time
        self.start_cpu_time = start_cpu_time
        self.end_cpu_time = end_cpu_time
        self.stack_id = stack_id
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count
        self.calls: List[int] = calls


class IntervalSampleNode:

    def __init__(
        self,
        start_time: int,
        end_time: int,
        start_cpu_time: int,
        end_cpu_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.start_time = start_time
        self.end_time = end_time
        self.start_cpu_time = start_cpu_time
        self.end_cpu_time = end_cpu_time
        self.stack_id = stack_id
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class SampleNode:

    def __init__(
        self,
        start_time: int,
        start_cpu_time: int,
        stack_id: int,
        alloc_size: int,
        alloc_count: int,
    ):
        self.start_time = start_time
        self.start_cpu_time = start_cpu_time
        self.stack_id = stack_id
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count


class ProfileSample:

    def __init__(
        self,
        tid: int,
        time: int,
        cpu_time: int,
        alloc_size: int,
        alloc_count: int,
        stack_id: int,
        calls: List[int] = None,
    ):
        self.type = "profile"
        self.tid = tid
        self.start_time = time
        self.start_cpu_time = cpu_time
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count
        self.stack_id = stack_id
        self.calls: List[int] = calls


class ProfileSampleNode:

    def __init__(
        self, time: int, cpu_time: int, alloc_size: int, alloc_count: int, stack_id: int
    ):
        self.start_time = time
        self.start_cpu_time = cpu_time
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count
        self.stack_id = stack_id


class TimeRangeSample:

    def __init__(
        self,
        tid: int,
        time: int,
        cpu_time: int,
        alloc_size: int,
        alloc_count: int,
        end_time: int,
        end_cpu_time: int,
        stack_id: int,
        calls: List[int] = None,
    ):
        self.type = "time_range"
        self.tid = tid
        self.start_time = time
        self.start_cpu_time = cpu_time
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count
        self.end_time = end_time
        self.end_cpu_time = end_cpu_time
        self.stack_id = stack_id
        self.calls: List[int] = calls


class TimeRangeSampleNode:

    def __init__(
        self,
        time: int,
        cpu_time: int,
        alloc_size: int,
        alloc_count: int,
        end_time: int,
        end_cpu_time: int,
        stack_id: int,
    ):
        self.start_time = time
        self.start_cpu_time = cpu_time
        self.alloc_size = alloc_size
        self.alloc_count = alloc_count
        self.stack_id = stack_id
        self.end_time = end_time
        self.end_cpu_time = end_cpu_time


def sample_info_key(cpu_sample_info):
    return (cpu_sample_info.start_time, cpu_sample_info.start_cpu_time)


class MethodNode:

    def __init__(
        self,
        address: int = 0,
        start_time: int = 0,
        end_time: int = 0,
        start_cpu_time: int = 0,
        end_cpu_time: int = 0,
        start_alloc_size: int = 0,
        end_alloc_size: int = 0,
        start_alloc_count: int = 0,
        end_alloc_count: int = 0,
    ):
        self.node_id = 0
        self.parent_id = 0
        self.address = address
        self.start_time = start_time
        self.end_time = end_time
        self.start_cpu_time = start_cpu_time
        self.end_cpu_time = end_cpu_time
        self.start_alloc_size = start_alloc_size
        self.end_alloc_size = end_alloc_size
        self.start_alloc_count = start_alloc_count
        self.end_alloc_count = end_alloc_count
        self.alloc_size = end_alloc_size - start_alloc_size
        self.alloc_count = end_alloc_count - start_alloc_count
        self.children: List[MethodNode] = []
        self.parent: MethodNode = None

    def update_cpu_time(self, cpu_time):
        if self.end_cpu_time < cpu_time:
            self.end_cpu_time = cpu_time

    def update_end_time(self, end_time):
        self.end_time = end_time

    def update_alloc_size(self, alloc_size):
        self.end_alloc_size = alloc_size
        self.alloc_size = alloc_size - self.start_alloc_size

    def update_alloc_count(self, alloc_count):
        self.end_alloc_count = alloc_count
        self.alloc_count = alloc_count - self.start_alloc_count


class MethodTree:

    def __init__(self, tid: int, start_time: int, main_thread: bool):
        self.root = MethodNode()
        self.tid = tid
        self.start_time = start_time
        self.main_thread = main_thread

    @staticmethod
    def put_child(node: MethodNode, parent: MethodNode):
        parent.children.append(node)
        node.parent = parent


class RetraceIOSSymbolRequest:

    def __init__(self):
        pass


class TraceRawContent:

    def __init__(
        self,
        bundle_id: str,
        start_time: int,
        cpu_sample_list: list[CPUSample],
        image_info_list: List[ImageInfo],
        thread_info_map: Dict[int, ThreadInfo],
        slice_info_map: Dict[str, List],
        counter_info_map: Dict[str, List[PerfettoCounterInfo]],
    ):
        self.bundle_id = bundle_id
        self.start_time = start_time
        self.cpu_sample_list = cpu_sample_list
        self.image_info_list = image_info_list
        self.thread_info_map = thread_info_map
        self.slice_info_map = slice_info_map
        self.counter_info_map = counter_info_map


class TraceParsedContent:

    def __init__(
        self, method_tree_list: list[MethodTree], addr_symbol_map: Dict[int, SymbolInfo]
    ):
        self.method_tree_list = method_tree_list
        self.addr_symbol_map = addr_symbol_map


class Statistics:

    def __init__(
        self,
        bundle_id: str = "",
        device_id: str = "",
        device_type: str = "",
        os_version: str = "",
        build_version: str = "",
        user_email: str = "",
        success: bool = False,
    ):
        self.bundle_id = bundle_id
        self.device_id = device_id
        self.device_type = device_type
        self.os_version = os_version
        self.build_version = build_version
        self.user_email = user_email
        self.success = success
