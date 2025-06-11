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

import json
import sqlite3
from typing import Dict, List, Optional, Tuple

import sortedcontainers

from btrace.model import (
    AsyncSample,
    AsyncSampleNode,
    DispatchSample,
    DispatchSampleNode,
    CPUSample,
    DateSample,
    DateSampleNode,
    ImageInfo,
    IntervalSampleNode,
    LockAction,
    LockSample,
    LockSampleNode,
    MemSample,
    MemSampleNode,
    PerfettoCounterInfo,
    PerfettoSliceInfo,
    ProfileSample,
    ProfileSampleNode,
    SampleNode,
    ThreadInfo,
    TimeRangeSample,
    TimeRangeSampleNode,
    TraceRawContent,
    image_info_key,
    sample_info_key,
)
from btrace.model import CallstackNode
from btrace import util

btrace_table_name = "BTraceModel"
image_info_table_name = "ImageInfoModel"
callstack_table_name = "CallStackTableModel"
thread_info_table_name = "ThreadInfoModel"
timeseries_info_table_name = "TimeSeriesModel"
cpu_sample_table_name = "CPUSampleModel"
cpu_batch_sample_table_name = "CPUBatchSampleModel"
cpu_interval_sample_table_name = "CPUIntervalSampleModel"
cpu_batch_interval_sample_table_name = "CPUBatchIntervalSampleModel"
mem_sample_table_name = "MemSampleModel"
mem_batch_sample_table_name = "MemBatchSampleModel"
date_sample_table_name = "DateSampleModel"
date_batch_sample_table_name = "DateBatchSampleModel"
lock_sample_table_name = "LockSampleModel"
lock_batch_sample_table_name = "LockBatchSampleModel"
async_sample_table_name = "AsyncSampleModel"
async_batch_sample_table_name = "AsyncBatchSampleModel"
dispatch_sample_table_name = "DispatchSampleModel"
dispatch_batch_sample_table_name = "DispatchBatchSampleModel"
profile_sample_table_name = "ProfileSampleModel"
profile_batch_sample_table_name = "ProfileBatchSampleModel"
time_range_sample_table_name = "TimeRangeSampleModel"
time_range_batch_sample_table_name = "TimeRangeBatchSampleModel"


def gen_callstack_map(callstack_table: bytes) -> Dict[int, CallstackNode]:
    result: Dict[int, CallstackNode] = {}
    step_size = 4
    for idx in range(0, len(callstack_table), 4 * step_size):
        stack_id = int.from_bytes(
            callstack_table[idx : idx + step_size], byteorder="little"
        )
        parent = int.from_bytes(
            callstack_table[idx + step_size : idx + 2 * step_size], byteorder="little"
        )
        address = int.from_bytes(
            callstack_table[idx + 2 * step_size : idx + 4 * step_size],
            byteorder="little",
        )

        result[stack_id] = CallstackNode(address, parent, stack_id)

    return result


def unwind_stack_from_callstack_table(
    callstack_table: Dict[int, CallstackNode], index_pos: int
):
    result = []

    if index_pos == 0:
        if util.verbose():
            print("Warning!, stack id == 0")
        return result

    node: CallstackNode = callstack_table[index_pos]

    while True:
        result.append(node.address)

        parent = node.parent
        if parent == 0:
            break

        node = callstack_table[parent]

    result.reverse()
    return result


def gen_async_sample_list(async_sample_bytes: bytes) -> List[AsyncSampleNode]:
    result: List[AsyncSampleNode] = []
    step_size = 4

    for idx in range(0, len(async_sample_bytes), 5 * step_size):
        source_time = int.from_bytes(
            async_sample_bytes[idx : idx + step_size], byteorder="little"
        )
        source_cpu_time = int.from_bytes(
            async_sample_bytes[idx + step_size : idx + 2 * step_size],
            byteorder="little",
        )
        target_tid = int.from_bytes(
            async_sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        target_time = int.from_bytes(
            async_sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            async_sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )

        result.append(
            AsyncSampleNode(
                source_time, source_cpu_time, target_tid, target_time, stack_id, 0, 0
            )
        )

    return result


def gen_async_sample_list_v2(async_sample_bytes: bytes) -> List[AsyncSampleNode]:
    result: List[AsyncSampleNode] = []
    step_size = 4

    for idx in range(0, len(async_sample_bytes), 7 * step_size):
        source_time = int.from_bytes(
            async_sample_bytes[idx : idx + step_size], byteorder="little"
        )
        source_cpu_time = int.from_bytes(
            async_sample_bytes[idx + step_size : idx + 2 * step_size],
            byteorder="little",
        )
        target_tid = int.from_bytes(
            async_sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        target_time = int.from_bytes(
            async_sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            async_sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )
        alloc_size = int.from_bytes(
            async_sample_bytes[idx + 5 * step_size : idx + 6 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            async_sample_bytes[idx + 6 * step_size : idx + 7 * step_size],
            byteorder="little",
        )

        result.append(
            AsyncSampleNode(
                source_time,
                source_cpu_time,
                target_tid,
                target_time,
                stack_id,
                alloc_size,
                alloc_count,
            )
        )

    return result


def gen_async_sample_list_v3(async_sample_bytes: bytes) -> List[AsyncSampleNode]:
    result = gen_async_sample_list_v2(async_sample_bytes)

    for node in result:
        node.start_cpu_time *= 10
        node.start_time *= 10
        node.target_time *= 10

    return result


def gen_dispatch_sample_list(dispatch_sample_bytes: bytes) -> List[DispatchSampleNode]:
    result: List[DispatchSampleNode] = []
    step_size = 4

    for idx in range(0, len(dispatch_sample_bytes), 5 * step_size):
        source_time = int.from_bytes(
            dispatch_sample_bytes[idx : idx + step_size], byteorder="little"
        )
        source_cpu_time = int.from_bytes(
            dispatch_sample_bytes[idx + step_size : idx + 2 * step_size],
            byteorder="little",
        )
        target_tid = int.from_bytes(
            dispatch_sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        target_time = int.from_bytes(
            dispatch_sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            dispatch_sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )

        result.append(
            DispatchSampleNode(
                source_time, source_cpu_time, target_tid, target_time, stack_id, 0, 0
            )
        )

    return result


def gen_dispatch_sample_list_v2(
    dispatch_sample_bytes: bytes,
) -> List[DispatchSampleNode]:
    result: List[DispatchSampleNode] = []
    step_size = 4

    for idx in range(0, len(dispatch_sample_bytes), 7 * step_size):
        source_time = int.from_bytes(
            dispatch_sample_bytes[idx : idx + step_size], byteorder="little"
        )
        source_cpu_time = int.from_bytes(
            dispatch_sample_bytes[idx + step_size : idx + 2 * step_size],
            byteorder="little",
        )
        target_tid = int.from_bytes(
            dispatch_sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        target_time = int.from_bytes(
            dispatch_sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            dispatch_sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )
        alloc_size = int.from_bytes(
            dispatch_sample_bytes[idx + 5 * step_size : idx + 6 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            dispatch_sample_bytes[idx + 6 * step_size : idx + 7 * step_size],
            byteorder="little",
        )

        result.append(
            DispatchSampleNode(
                source_time,
                source_cpu_time,
                target_tid,
                target_time,
                stack_id,
                alloc_size,
                alloc_count,
            )
        )

    return result


def gen_dispatch_sample_list_v3(
    dispatch_sample_bytes: bytes,
) -> List[DispatchSampleNode]:
    result = gen_dispatch_sample_list_v2(dispatch_sample_bytes)

    for node in result:
        node.start_cpu_time *= 10
        node.start_time *= 10
        node.target_time *= 10

    return result


def gen_dispatch_sample_list_v4(
    dispatch_sample_bytes: bytes,
) -> List[DispatchSampleNode]:
    result: List[DispatchSampleNode] = []
    step_size = 4

    for idx in range(0, len(dispatch_sample_bytes), 7 * step_size):
        time = int.from_bytes(
            dispatch_sample_bytes[idx : idx + step_size], byteorder="little"
        )
        cpu_time = int.from_bytes(
            dispatch_sample_bytes[idx + step_size : idx + 2 * step_size],
            byteorder="little",
        )
        alloc_size = int.from_bytes(
            dispatch_sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            dispatch_sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            dispatch_sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )
        target_tid = int.from_bytes(
            dispatch_sample_bytes[idx + 5 * step_size : idx + 6 * step_size],
            byteorder="little",
        )
        target_time = int.from_bytes(
            dispatch_sample_bytes[idx + 6 * step_size : idx + 7 * step_size],
            byteorder="little",
        )
        
        time *= 10
        cpu_time *= 10
        target_time *= 10

        result.append(
            DispatchSampleNode(
                time,
                cpu_time,
                target_tid,
                target_time,
                stack_id,
                alloc_size,
                alloc_count
            )
        )

    return result


def gen_lock_sample(sample: bytes) -> LockSample:
    step_size = 4

    id = int.from_bytes(sample[0:6], byteorder="little")
    action = int.from_bytes(sample[6 : 2 * step_size], byteorder="little")
    time = int.from_bytes(sample[2 * step_size : 3 * step_size], byteorder="little")
    cpu_time = int.from_bytes(sample[3 * step_size : 4 * step_size], byteorder="little")
    stack_id = int.from_bytes(sample[4 * step_size : 5 * step_size], byteorder="little")
    tid = int.from_bytes(sample[5 * step_size : 6 * step_size], byteorder="little")

    result = LockSample(tid, id, action, time, cpu_time, stack_id, 0, 0)

    return result


def gen_lock_sample_v2(sample: bytes) -> LockSample:
    step_size = 4

    id = int.from_bytes(sample[0:6], byteorder="little")
    action = int.from_bytes(sample[6 : 2 * step_size], byteorder="little")
    time = int.from_bytes(sample[2 * step_size : 3 * step_size], byteorder="little")
    cpu_time = int.from_bytes(sample[3 * step_size : 4 * step_size], byteorder="little")
    stack_id = int.from_bytes(sample[4 * step_size : 5 * step_size], byteorder="little")
    tid = int.from_bytes(sample[5 * step_size : 6 * step_size], byteorder="little")
    alloc_size = int.from_bytes(
        sample[6 * step_size : 7 * step_size], byteorder="little"
    )
    alloc_count = int.from_bytes(
        sample[7 * step_size : 8 * step_size], byteorder="little"
    )

    result = LockSample(
        tid, id, action, time, cpu_time, stack_id, alloc_size, alloc_count
    )

    return result


def gen_lock_sample_v3(sample: bytes) -> LockSample:
    result = gen_lock_sample_v2(sample)

    result.start_cpu_time *= 10
    result.start_time *= 10

    return result


def gen_lock_sample_v4(sample: bytes) -> LockSample:
    step_size = 4

    time = int.from_bytes(sample[0 : step_size], byteorder="little")
    cpu_time = int.from_bytes(sample[step_size : 2 * step_size], byteorder="little")
    alloc_size = int.from_bytes(sample[2 * step_size : 3 * step_size], byteorder="little")
    alloc_count = int.from_bytes(sample[3 * step_size : 4 * step_size], byteorder="little")
    stack_id = int.from_bytes(
        sample[4 * step_size : 5 * step_size], byteorder="little"
    )
    tid = int.from_bytes(
        sample[5 * step_size : 6 * step_size], byteorder="little"
    )
    id = int.from_bytes(sample[6 * step_size : 6 * step_size + 6], byteorder="little")
    action= int.from_bytes(sample[6 * step_size + 6 : 8 * step_size], byteorder="little")
    
    time *= 10
    cpu_time *= 10

    result = LockSample(
        tid, id, action, time, cpu_time, stack_id, alloc_size, alloc_count
    )

    return result


def gen_lock_sample_list(sample_bytes: bytes) -> List[LockSampleNode]:
    result: List[LockSampleNode] = []
    step_size = 4

    for idx in range(0, len(sample_bytes), 5 * step_size):
        id = int.from_bytes(sample_bytes[idx : idx + 6], byteorder="little")
        action = int.from_bytes(
            sample_bytes[idx + 6 : idx + 2 * step_size], byteorder="little"
        )
        time = int.from_bytes(
            sample_bytes[idx + 2 * step_size : idx + 3 * step_size], byteorder="little"
        )
        cpu_time = int.from_bytes(
            sample_bytes[idx + 3 * step_size : idx + 4 * step_size], byteorder="little"
        )
        stack_id = int.from_bytes(
            sample_bytes[idx + 4 * step_size : idx + 5 * step_size], byteorder="little"
        )

        result.append(LockSampleNode(id, action, time, cpu_time, stack_id, 0, 0))

    return result


def gen_lock_sample_list_v2(sample_bytes: bytes) -> List[LockSampleNode]:
    result: List[LockSampleNode] = []
    step_size = 4

    for idx in range(0, len(sample_bytes), 7 * step_size):
        id = int.from_bytes(sample_bytes[idx : idx + 6], byteorder="little")
        action = int.from_bytes(
            sample_bytes[idx + 6 : idx + 2 * step_size], byteorder="little"
        )
        time = int.from_bytes(
            sample_bytes[idx + 2 * step_size : idx + 3 * step_size], byteorder="little"
        )
        cpu_time = int.from_bytes(
            sample_bytes[idx + 3 * step_size : idx + 4 * step_size], byteorder="little"
        )
        stack_id = int.from_bytes(
            sample_bytes[idx + 4 * step_size : idx + 5 * step_size], byteorder="little"
        )
        alloc_size = int.from_bytes(
            sample_bytes[idx + 5 * step_size : idx + 6 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            sample_bytes[idx + 6 * step_size : idx + 7 * step_size],
            byteorder="little",
        )

        result.append(
            LockSampleNode(
                id, action, time, cpu_time, stack_id, alloc_size, alloc_count
            )
        )

    return result


def gen_lock_sample_list_v3(sample_bytes: bytes) -> List[LockSampleNode]:
    result = gen_lock_sample_list_v2(sample_bytes)

    for node in result:
        node.start_time *= 10
        node.start_cpu_time *= 10

    return result


def gen_lock_sample_list_v4(sample_bytes: bytes) -> List[LockSampleNode]:
    result: List[LockSampleNode] = []
    step_size = 4

    for idx in range(0, len(sample_bytes), 7 * step_size):
        time = int.from_bytes(
            sample_bytes[idx: idx + step_size], byteorder="little"
        )
        cpu_time = int.from_bytes(
            sample_bytes[idx + step_size : idx + 2 * step_size], byteorder="little"
        )
        alloc_size = int.from_bytes(
            sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )        
        stack_id = int.from_bytes(
            sample_bytes[idx + 4 * step_size : idx + 5 * step_size], byteorder="little"
        )
        id = int.from_bytes(sample_bytes[idx + 5 * step_size : idx + 5 * step_size + 6], byteorder="little")
        action = int.from_bytes(
            sample_bytes[idx + 5 * step_size + 6 : idx + 7 * step_size], byteorder="little"
        )
        
        time *= 10
        cpu_time *= 10

        result.append(
            LockSampleNode(
                id, action, time, cpu_time, stack_id, alloc_size, alloc_count
            )
        )

    return result


def gen_date_sample_list(sample_bytes: bytes) -> List[DateSampleNode]:
    result: List[SampleNode] = []
    step_size = 4

    for idx in range(0, len(sample_bytes), 3 * step_size):
        time = int.from_bytes(sample_bytes[idx : idx + step_size], byteorder="little")
        cpu_time = int.from_bytes(
            sample_bytes[idx + step_size : idx + 2 * step_size], byteorder="little"
        )
        stack_id = int.from_bytes(
            sample_bytes[idx + 2 * step_size : idx + 3 * step_size], byteorder="little"
        )

        result.append(DateSampleNode(time, cpu_time, stack_id, 0, 0))

    return result


def gen_date_sample_list_v2(sample_bytes: bytes) -> List[DateSampleNode]:
    result: List[SampleNode] = []
    step_size = 4

    for idx in range(0, len(sample_bytes), 5 * step_size):
        time = int.from_bytes(sample_bytes[idx : idx + step_size], byteorder="little")
        cpu_time = int.from_bytes(
            sample_bytes[idx + step_size : idx + 2 * step_size], byteorder="little"
        )
        stack_id = int.from_bytes(
            sample_bytes[idx + 2 * step_size : idx + 3 * step_size], byteorder="little"
        )
        alloc_size = int.from_bytes(
            sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )

        result.append(DateSampleNode(time, cpu_time, stack_id, alloc_size, alloc_count))

    return result


def gen_date_sample_list_v3(sample_bytes: bytes) -> List[DateSampleNode]:
    result = gen_date_sample_list_v2(sample_bytes)

    for node in result:
        node.start_time *= 10
        node.start_cpu_time *= 10

    return result


def gen_mem_sample_list(mem_sample_bytes: bytes) -> List[MemSampleNode]:
    result: List[MemSampleNode] = []
    step_size = 4
    for idx in range(0, len(mem_sample_bytes), 5 * step_size):
        addr = int.from_bytes(
            mem_sample_bytes[idx : idx + step_size], byteorder="little"
        )
        size = int.from_bytes(
            mem_sample_bytes[idx + step_size : idx + 2 * step_size], byteorder="little"
        )
        time = int.from_bytes(
            mem_sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        cpu_time = int.from_bytes(
            mem_sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            mem_sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )

        result.append(MemSampleNode(addr, size, time, cpu_time, stack_id, 0, 0))

    return result


def gen_mem_sample_list_v2(mem_sample_bytes: bytes) -> List[MemSampleNode]:
    result: List[MemSampleNode] = []
    step_size = 4
    for idx in range(0, len(mem_sample_bytes), 7 * step_size):
        addr = int.from_bytes(
            mem_sample_bytes[idx : idx + step_size], byteorder="little"
        )
        size = int.from_bytes(
            mem_sample_bytes[idx + step_size : idx + 2 * step_size], byteorder="little"
        )
        time = int.from_bytes(
            mem_sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        cpu_time = int.from_bytes(
            mem_sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            mem_sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )
        alloc_size = int.from_bytes(
            mem_sample_bytes[idx + 5 * step_size : idx + 6 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            mem_sample_bytes[idx + 6 * step_size : idx + 7 * step_size],
            byteorder="little",
        )

        result.append(
            MemSampleNode(addr, size, time, cpu_time, stack_id, alloc_size, alloc_count)
        )

    return result


def gen_mem_sample_list_v3(mem_sample_bytes: bytes) -> List[MemSampleNode]:
    result = gen_mem_sample_list_v2(mem_sample_bytes)

    for node in result:
        node.start_time *= 10
        node.start_cpu_time *= 10

    return result


def gen_interval_sample_list(interval_sample_bytes: bytes) -> List[IntervalSampleNode]:
    result: List[IntervalSampleNode] = []
    step_size = 4
    for idx in range(0, len(interval_sample_bytes), 5 * step_size):
        start_time = int.from_bytes(
            interval_sample_bytes[idx : idx + step_size], byteorder="little"
        )
        start_cpu_time = int.from_bytes(
            interval_sample_bytes[idx + step_size : idx + 2 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            interval_sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        end_time = int.from_bytes(
            interval_sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        end_cpu_time = int.from_bytes(
            interval_sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )

        result.append(
            IntervalSampleNode(
                start_time, end_time, start_cpu_time, end_cpu_time, stack_id, 0, 0
            )
        )

    return result


def gen_interval_sample_list_v2(
    interval_sample_bytes: bytes,
) -> List[IntervalSampleNode]:
    result: List[IntervalSampleNode] = []
    step_size = 4
    for idx in range(0, len(interval_sample_bytes), 7 * step_size):
        start_time = int.from_bytes(
            interval_sample_bytes[idx : idx + step_size], byteorder="little"
        )
        start_cpu_time = int.from_bytes(
            interval_sample_bytes[idx + step_size : idx + 2 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            interval_sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        end_time = int.from_bytes(
            interval_sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        end_cpu_time = int.from_bytes(
            interval_sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )
        alloc_size = int.from_bytes(
            interval_sample_bytes[idx + 5 * step_size : idx + 6 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            interval_sample_bytes[idx + 6 * step_size : idx + 7 * step_size],
            byteorder="little",
        )

        result.append(
            IntervalSampleNode(
                start_time,
                end_time,
                start_cpu_time,
                end_cpu_time,
                stack_id,
                alloc_size,
                alloc_count,
            )
        )

    return result


def gen_interval_sample_list_v3(
    interval_sample_bytes: bytes,
) -> List[IntervalSampleNode]:
    result = gen_interval_sample_list_v2(interval_sample_bytes)

    for node in result:
        node.start_time *= 10
        node.end_time *= 10
        node.start_cpu_time *= 10
        node.end_cpu_time *= 10

    return result


def gen_profile_sample_list(sample_bytes: bytes) -> List[ProfileSampleNode]:
    result: List[ProfileSampleNode] = []
    step_size = 4
    for idx in range(0, len(sample_bytes), 5 * step_size):
        time = int.from_bytes(sample_bytes[idx : idx + step_size], byteorder="little")
        cpu_time = int.from_bytes(
            sample_bytes[idx + step_size : idx + 2 * step_size],
            byteorder="little",
        )
        alloc_size = int.from_bytes(
            sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )

        time *= 10
        cpu_time *= 10

        result.append(
            ProfileSampleNode(time, cpu_time, alloc_size, alloc_count, stack_id)
        )

    return result


def gen_time_range_sample_list_v4(sample_bytes: bytes) -> List[TimeRangeSampleNode]:
    result: List[TimeRangeSampleNode] = []
    step_size = 4
    for idx in range(0, len(sample_bytes), 7 * step_size):
        time = int.from_bytes(sample_bytes[idx : idx + step_size], byteorder="little")
        cpu_time = int.from_bytes(
            sample_bytes[idx + step_size : idx + 2 * step_size],
            byteorder="little",
        )
        alloc_size = int.from_bytes(
            sample_bytes[idx + 2 * step_size : idx + 3 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        stack_id = int.from_bytes(
            sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )
        end_time = int.from_bytes(
            sample_bytes[idx + 5 * step_size : idx + 6 * step_size], byteorder="little"
        )
        end_cpu_time = int.from_bytes(
            sample_bytes[idx + 6 * step_size : idx + 7 * step_size],
            byteorder="little",
        )

        time *= 10
        cpu_time *= 10
        end_time *= 10
        end_cpu_time *= 10

        result.append(
            TimeRangeSampleNode(
                time,
                cpu_time,
                alloc_size,
                alloc_count,
                end_time,
                end_cpu_time,
                stack_id
            )
        )

    return result


def gen_cpu_sample_list(sample_bytes: bytes) -> List[SampleNode]:
    result: List[SampleNode] = []
    step_size = 4
    for idx in range(0, len(sample_bytes), 3 * step_size):
        start_time = int.from_bytes(
            sample_bytes[idx : idx + step_size], byteorder="little"
        )
        start_cpu_time = int.from_bytes(
            sample_bytes[idx + step_size : idx + 2 * step_size], byteorder="little"
        )
        stack_id = int.from_bytes(
            sample_bytes[idx + 2 * step_size : idx + 3 * step_size], byteorder="little"
        )

        result.append(SampleNode(start_time, start_cpu_time, stack_id, 0, 0))

    return result


def gen_cpu_sample_list_v2(sample_bytes: bytes) -> List[SampleNode]:
    result: List[SampleNode] = []
    step_size = 4
    for idx in range(0, len(sample_bytes), 5 * step_size):
        start_time = int.from_bytes(
            sample_bytes[idx : idx + step_size], byteorder="little"
        )
        start_cpu_time = int.from_bytes(
            sample_bytes[idx + step_size : idx + 2 * step_size], byteorder="little"
        )
        stack_id = int.from_bytes(
            sample_bytes[idx + 2 * step_size : idx + 3 * step_size], byteorder="little"
        )
        alloc_size = int.from_bytes(
            sample_bytes[idx + 3 * step_size : idx + 4 * step_size],
            byteorder="little",
        )
        alloc_count = int.from_bytes(
            sample_bytes[idx + 4 * step_size : idx + 5 * step_size],
            byteorder="little",
        )

        result.append(
            SampleNode(start_time, start_cpu_time, stack_id, alloc_size, alloc_count)
        )

    return result


def gen_cpu_sample_list_v3(sample_bytes: bytes) -> List[SampleNode]:
    result = gen_cpu_sample_list_v2(sample_bytes)

    for node in result:
        node.start_time *= 10
        node.start_cpu_time *= 10

    return result


def table_exists(con: sqlite3.Connection, table_name: str) -> bool:
    table_exist_sql = f'select count(*) from sqlite_master where type="table" and name = "{table_name}";'
    res = con.execute(table_exist_sql)
    return 0 < res.fetchone()["count(*)"]


def read_cpu_samples(
    con: sqlite3.Connection,
    trace_id: int,
    callstack_table: Dict[int, CallstackNode],
    version: int,
) -> Dict[int, List[CPUSample]]:
    cpu_sample_map: Dict[int, List[CPUSample]] = {}

    if table_exists(con, cpu_interval_sample_table_name):
        # cpu interval sample
        def cpu_interval_sample(row) -> CPUSample:
            tid = row["tid"]
            start_time = int(row["start_time"])
            end_time = int(row["end_time"])
            start_cpu_time = int(row["start_cpu_time"])
            end_cpu_time = int(row["end_cpu_time"])
            stack_id = int(row["stack_id"])

            cpu_sample = CPUSample(
                tid, start_time, end_time, start_cpu_time, end_cpu_time, stack_id, 0, 0
            )

            return cpu_sample

        def cpu_interval_sample_v2(row) -> CPUSample:
            tid = row["tid"]
            start_time = int(row["start_time"])
            end_time = int(row["end_time"])
            start_cpu_time = int(row["start_cpu_time"])
            end_cpu_time = int(row["end_cpu_time"])
            stack_id = int(row["stack_id"])
            alloc_size = int(row["alloc_size"])
            alloc_count = int(row["alloc_count"])

            cpu_sample = CPUSample(
                tid,
                start_time,
                end_time,
                start_cpu_time,
                end_cpu_time,
                stack_id,
                alloc_size,
                alloc_count,
            )

            return cpu_sample

        def cpu_interval_sample_v3(row) -> CPUSample:
            cpu_sample = cpu_interval_sample_v2(row)
            cpu_sample.start_time *= 10
            cpu_sample.end_time *= 10
            cpu_sample.start_cpu_time *= 10
            cpu_sample.end_cpu_time *= 10

            return cpu_sample

        cpu_interval_sample_sql = (
            f"select * from {cpu_interval_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(cpu_interval_sample_sql)

        for one in res.fetchall():
            cpu_sample: Optional[CPUSample] = None

            if version == 1:
                cpu_sample = cpu_interval_sample(one)
            elif version == 2:
                cpu_sample = cpu_interval_sample_v2(one)
            elif version == 3:
                cpu_sample = cpu_interval_sample_v3(one)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            tid = cpu_sample.tid
            tid_trace_list = cpu_sample_map.get(tid) or []
            tid_trace_list.append(cpu_sample)
            cpu_sample_map[tid] = tid_trace_list

    if table_exists(con, cpu_sample_table_name):
        # cpu sample
        def gen_cpu_sample(row) -> CPUSample:
            tid = row["tid"]
            start_time = int(row["start_time"])
            end_time = start_time
            start_cpu_time = int(row["start_cpu_time"])
            end_cpu_time = start_cpu_time
            stack_id = int(row["stack_id"])

            cpu_sample = CPUSample(
                tid, start_time, end_time, start_cpu_time, end_cpu_time, stack_id, 0, 0
            )

            return cpu_sample

        def gen_cpu_sample_v2(row) -> CPUSample:
            tid = row["tid"]
            start_time = int(row["start_time"])
            end_time = start_time
            start_cpu_time = int(row["start_cpu_time"])
            end_cpu_time = start_cpu_time
            stack_id = int(row["stack_id"])
            alloc_size = int(row["alloc_size"])
            alloc_count = int(row["alloc_count"])

            cpu_sample = CPUSample(
                tid,
                start_time,
                end_time,
                start_cpu_time,
                end_cpu_time,
                stack_id,
                alloc_size,
                alloc_count,
            )

            return cpu_sample

        def gen_cpu_sample_v3(row) -> CPUSample:
            cpu_sample = gen_cpu_sample_v2(row)
            cpu_sample.start_time *= 10
            cpu_sample.end_time *= 10
            cpu_sample.start_cpu_time *= 10
            cpu_sample.end_cpu_time *= 10

            return cpu_sample

        cpu_sample_sql = (
            f"select * from {cpu_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(cpu_sample_sql)

        for one in res.fetchall():
            cpu_sample: Optional[CPUSample] = None

            if version == 1:
                cpu_sample = gen_cpu_sample(one)
            elif version == 2:
                cpu_sample = gen_cpu_sample_v2(one)
            elif version == 3:
                cpu_sample = gen_cpu_sample_v3(one)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            tid = cpu_sample.tid
            tid_trace_list = cpu_sample_map.get(tid) or []
            tid_trace_list.append(cpu_sample)
            cpu_sample_map[tid] = tid_trace_list

    if table_exists(con, cpu_batch_interval_sample_table_name):
        cpu_interval_sample_sql = f"select * from {cpu_batch_interval_sample_table_name} where trace_id={trace_id};"
        res = con.execute(cpu_interval_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            tid = one["tid"]
            interval_sample_bytes: bytes = one["nodes"]
            interval_sample_list: Optional[List[IntervalSampleNode]] = None

            if version == 1:
                interval_sample_list = gen_interval_sample_list(interval_sample_bytes)
            elif version == 2:
                interval_sample_list = gen_interval_sample_list_v2(
                    interval_sample_bytes
                )
            elif version == 3:
                interval_sample_list = gen_interval_sample_list_v3(
                    interval_sample_bytes
                )
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            for cpu_sample in interval_sample_list:
                start_time = cpu_sample.start_time
                end_time = cpu_sample.end_time
                start_cpu_time = cpu_sample.start_cpu_time
                end_cpu_time = cpu_sample.end_cpu_time
                stack_id = cpu_sample.stack_id
                alloc_size = cpu_sample.alloc_size
                alloc_count = cpu_sample.alloc_count

                cpu_sample = CPUSample(
                    tid,
                    start_time,
                    end_time,
                    start_cpu_time,
                    end_cpu_time,
                    stack_id,
                    alloc_size,
                    alloc_count,
                )

                tid_trace_list = cpu_sample_map.get(tid) or []
                tid_trace_list.append(cpu_sample)
                cpu_sample_map[tid] = tid_trace_list

    if table_exists(con, cpu_batch_sample_table_name):
        cpu_sample_sql = (
            f"select * from {cpu_batch_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(cpu_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            tid = one["tid"]
            sample_bytes: bytes = one["nodes"]
            sample_list: Optional[List[SampleNode]] = None

            if version == 1:
                sample_list = gen_cpu_sample_list(sample_bytes)
            elif version == 2:
                sample_list = gen_cpu_sample_list_v2(sample_bytes)
            elif version == 3:
                sample_list = gen_cpu_sample_list_v3(sample_bytes)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            for cpu_sample in sample_list:
                start_time = cpu_sample.start_time
                end_time = start_time
                start_cpu_time = cpu_sample.start_cpu_time
                end_cpu_time = start_cpu_time
                stack_id = cpu_sample.stack_id
                alloc_size = cpu_sample.alloc_size
                alloc_count = cpu_sample.alloc_count

                cpu_sample = CPUSample(
                    tid,
                    start_time,
                    end_time,
                    start_cpu_time,
                    end_cpu_time,
                    stack_id,
                    alloc_size,
                    alloc_count,
                )

                tid_trace_list = cpu_sample_map.get(tid) or []
                tid_trace_list.append(cpu_sample)
                cpu_sample_map[tid] = tid_trace_list

    for tid, tid_trace_list in cpu_sample_map.items():

        for idx, cpu_sample in enumerate(tid_trace_list):
            stack = []
            stack_id = cpu_sample.stack_id
            stack = unwind_stack_from_callstack_table(callstack_table, stack_id)

            if len(stack) == 0 or stack[0] == 0:
                # print(f"error: stack_id: {index_pos}")
                continue

            cpu_sample.calls = stack

    return cpu_sample_map


def read_profile_samples(
    con: sqlite3.Connection,
    trace_id: int,
    callstack_table: Dict[int, CallstackNode],
    version: int,
) -> Dict[int, List[ProfileSample]]:
    profile_sample_map: Dict[int, List[ProfileSample]] = {}

    if table_exists(con, profile_sample_table_name):
        # profile sample
        def gen_profile_sample(row) -> ProfileSample:
            tid = row["tid"]
            time = int(row["time"]) * 10
            cpu_time = int(row["cpu_time"]) * 10
            alloc_size = int(row["alloc_size"])
            alloc_count = int(row["alloc_count"])
            stack_id = int(row["stack_id"])

            profile_sample = ProfileSample(
                tid, time, cpu_time, alloc_size, alloc_count, stack_id
            )

            return profile_sample

        profile_sample_sql = (
            f"select * from {profile_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(profile_sample_sql)

        for one in res.fetchall():
            profile_sample: Optional[ProfileSample] = None

            if version == 4:
                profile_sample = gen_profile_sample(one)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            tid = profile_sample.tid
            tid_trace_list = profile_sample_map.get(tid) or []
            tid_trace_list.append(profile_sample)
            profile_sample_map[tid] = tid_trace_list

    if table_exists(con, profile_batch_sample_table_name):
        profile_batch_sample_sql = f"select * from {profile_batch_sample_table_name} where trace_id={trace_id};"
        res = con.execute(profile_batch_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            tid = one["tid"]
            sample_bytes: bytes = one["nodes"]
            sample_list: Optional[List[ProfileSampleNode]] = None

            if version == 4:
                sample_list = gen_profile_sample_list(sample_bytes)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            for sample_node in sample_list:
                time = sample_node.start_time
                cpu_time = sample_node.start_cpu_time
                alloc_size = sample_node.alloc_size
                alloc_count = sample_node.alloc_count
                stack_id = sample_node.stack_id

                profile_sample = ProfileSample(
                    tid, time, cpu_time, alloc_size, alloc_count, stack_id
                )

                tid_trace_list = profile_sample_map.get(tid) or []
                tid_trace_list.append(profile_sample)
                profile_sample_map[tid] = tid_trace_list

    for tid, tid_trace_list in profile_sample_map.items():

        for idx, profile_sample in enumerate(tid_trace_list):
            stack = []
            stack_id = profile_sample.stack_id
            stack = unwind_stack_from_callstack_table(callstack_table, stack_id)

            if len(stack) == 0 or stack[0] == 0:
                # print(f"error: stack_id: {index_pos}")
                continue

            profile_sample.calls = stack

    return profile_sample_map


def read_time_range_samples(
    con: sqlite3.Connection,
    trace_id: int,
    callstack_table: Dict[int, CallstackNode],
    version: int,
) -> Dict[int, List[TimeRangeSample]]:
    time_range_sample_map: Dict[int, List[TimeRangeSample]] = {}

    if table_exists(con, time_range_sample_table_name):
        # profile sample
        def gen_time_range_sample(row) -> TimeRangeSample:
            tid = row["tid"]
            time = int(row["time"]) * 10
            cpu_time = int(row["cpu_time"]) * 10
            alloc_size = int(row["alloc_size"])
            alloc_count = int(row["alloc_count"])
            end_time = int(row["end_time"]) * 10
            end_cpu_time = int(row["end_cpu_time"]) * 10
            stack_id = int(row["stack_id"])

            time_range_sample = TimeRangeSample(
                tid,
                time,
                cpu_time,
                alloc_size,
                alloc_count,
                end_time,
                end_cpu_time,
                stack_id,
            )

            return time_range_sample

        time_range_sample_sql = (
            f"select * from {time_range_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(time_range_sample_sql)

        for one in res.fetchall():
            time_range_sample: Optional[TimeRangeSample] = None

            if version == 4:
                time_range_sample = gen_time_range_sample(one)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            tid = time_range_sample.tid
            tid_trace_list = time_range_sample_map.get(tid) or []
            tid_trace_list.append(time_range_sample)
            time_range_sample_map[tid] = tid_trace_list

    if table_exists(con, time_range_batch_sample_table_name):
        time_range_batch_sample_sql = f"select * from {time_range_batch_sample_table_name} where trace_id={trace_id};"
        res = con.execute(time_range_batch_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            tid = one["tid"]
            sample_bytes: bytes = one["nodes"]
            sample_list: Optional[List[TimeRangeSampleNode]] = None

            if version == 4:
                sample_list = gen_time_range_sample_list_v4(sample_bytes)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            for sample_node in sample_list:
                time = sample_node.start_time
                cpu_time = sample_node.start_cpu_time
                alloc_size = sample_node.alloc_size
                alloc_count = sample_node.alloc_count
                end_time = sample_node.end_time
                end_cpu_time = sample_node.end_cpu_time
                stack_id = sample_node.stack_id

                time_range_sample = TimeRangeSample(
                    tid,
                    time,
                    cpu_time,
                    alloc_size,
                    alloc_count,
                    end_time,
                    end_cpu_time,
                    stack_id
                )

                tid_trace_list = time_range_sample_map.get(tid) or []
                tid_trace_list.append(time_range_sample)
                time_range_sample_map[tid] = tid_trace_list

    for tid, tid_trace_list in time_range_sample_map.items():

        for idx, time_range_sample in enumerate(tid_trace_list):
            stack = []
            stack_id = time_range_sample.stack_id
            stack = unwind_stack_from_callstack_table(callstack_table, stack_id)

            if len(stack) == 0 or stack[0] == 0:
                # print(f"error: stack_id: {index_pos}")
                continue

            time_range_sample.calls = stack

    return time_range_sample_map


def read_mem_samples(
    con: sqlite3.Connection,
    trace_id: int,
    callstack_table: Dict[int, CallstackNode],
    version: int,
) -> Dict[int, List[MemSample]]:
    mem_sample_map: Dict[int, List[MemSample]] = {}

    if table_exists(con, mem_sample_table_name):

        def gen_mem_sample(row) -> MemSample:
            tid = one["tid"]
            alloc_size = int(row["size"])
            start_time = int(row["time"])
            cpu_time = int(row["cpu_time"])
            stack_id = int(row["stack_id"])

            mem_sample = MemSample(
                tid, alloc_size, start_time, cpu_time, stack_id, 0, 0
            )

            return mem_sample

        def gen_mem_sample_v2(row) -> MemSample:
            tid = row["tid"]
            size = int(row["size"])
            start_time = int(row["time"])
            cpu_time = int(row["cpu_time"])
            stack_id = int(row["stack_id"])
            alloc_size = int(row["alloc_size"])
            alloc_count = int(row["alloc_count"])

            mem_sample = MemSample(
                tid, size, start_time, cpu_time, stack_id, alloc_size, alloc_count
            )

            return mem_sample

        def gen_mem_sample_v3(row) -> MemSample:
            mem_sample = gen_mem_sample_v2(row)
            mem_sample.start_time *= 10
            mem_sample.start_cpu_time *= 10

            return mem_sample

        mem_sample_sql = (
            f"select * from {mem_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(mem_sample_sql)

        for one in res.fetchall():
            mem_sample: Optional[MemSample] = None

            if version == 1:
                mem_sample = gen_mem_sample(one)
            elif version == 2:
                mem_sample = gen_mem_sample_v2(one)
            elif version == 3:
                mem_sample = gen_mem_sample_v3(one)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            tid = mem_sample.tid
            tid_trace_list = mem_sample_map.get(tid) or []
            tid_trace_list.append(mem_sample)
            mem_sample_map[tid] = tid_trace_list

    if table_exists(con, mem_batch_sample_table_name):
        mem_sample_sql = (
            f"select * from {mem_batch_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(mem_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            tid = one["tid"]
            mem_sample_bytes: bytes = one["nodes"]
            mem_sample_list: Optional[List[MemSampleNode]] = None

            if version == 1:
                mem_sample_list = gen_mem_sample_list(mem_sample_bytes)
            elif version == 2:
                mem_sample_list = gen_mem_sample_list_v2(mem_sample_bytes)
            elif version == 3:
                mem_sample_list = gen_mem_sample_list_v3(mem_sample_bytes)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            for mem_sample in mem_sample_list:
                alloc_size = int(mem_sample.size)
                start_time = mem_sample.start_time
                cpu_time = mem_sample.start_cpu_time
                stack_id = mem_sample.stack_id
                alloc_size = mem_sample.alloc_size
                alloc_count = mem_sample.alloc_count

                mem_sample = MemSample(
                    tid,
                    alloc_size,
                    start_time,
                    cpu_time,
                    stack_id,
                    alloc_size,
                    alloc_count,
                )

                tid_trace_list = mem_sample_map.get(tid) or []
                tid_trace_list.append(mem_sample)
                mem_sample_map[tid] = tid_trace_list

    for tid, tid_trace_list in mem_sample_map.items():
        for idx, mem_sample in enumerate(tid_trace_list):
            stack_id = mem_sample.stack_id
            stack = unwind_stack_from_callstack_table(callstack_table, stack_id)

            if len(stack) == 0 or stack[0] == 0:
                continue

            mem_sample.calls = stack

    return mem_sample_map


def read_async_samples(
    con: sqlite3.Connection,
    trace_id: int,
    callstack_table: Dict[int, CallstackNode],
    version: int,
) -> Dict[int, List[AsyncSample]]:

    async_sample_map: Dict[int, List[AsyncSample]] = {}

    if table_exists(con, async_sample_table_name):

        def gen_async_sample(row) -> AsyncSample:
            tid = row["source_tid"]
            source_time = int(row["source_time"])
            source_cpu_time = int(row["source_cpu_time"])
            target_tid = int(row["target_tid"])
            target_time = int(row["target_time"])
            stack_id = int(row["stack_id"])

            async_sample = AsyncSample(
                tid,
                source_time,
                source_cpu_time,
                target_tid,
                target_time,
                stack_id,
                0,
                0,
            )

            return async_sample

        def gen_async_sample_v2(row) -> AsyncSample:
            tid = row["source_tid"]
            source_time = int(row["source_time"])
            source_cpu_time = int(row["source_cpu_time"])
            target_tid = int(row["target_tid"])
            target_time = int(row["target_time"])
            stack_id = int(row["stack_id"])
            alloc_size = int(row["alloc_size"])
            alloc_count = int(row["alloc_count"])

            async_sample = AsyncSample(
                tid,
                source_time,
                source_cpu_time,
                target_tid,
                target_time,
                stack_id,
                alloc_size,
                alloc_count,
            )

            return async_sample

        def gen_async_sample_v3(row) -> AsyncSample:
            async_sample = gen_async_sample_v2(row)
            async_sample.start_time *= 10
            async_sample.target_time *= 10
            async_sample.start_cpu_time *= 10

            return async_sample

        async_sample_sql = (
            f"select * from {async_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(async_sample_sql)

        for one in res.fetchall():
            async_sample: Optional[AsyncSample] = None

            if version == 1:
                async_sample = gen_async_sample(one)
            elif version == 2:
                async_sample = gen_async_sample_v2(one)
            elif version == 3:
                async_sample = gen_async_sample_v3(one)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            tid = async_sample.tid
            tid_trace_list = async_sample_map.get(tid) or []
            tid_trace_list.append(async_sample)
            async_sample_map[tid] = tid_trace_list

    if table_exists(con, async_batch_sample_table_name):
        async_sample_sql = (
            f"select * from {async_batch_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(async_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            tid = one["tid"]
            async_sample_bytes: bytes = one["nodes"]
            async_sample_list: Optional[List[AsyncSampleNode]] = None

            if version == 1:
                async_sample_list = gen_async_sample_list(async_sample_bytes)
            elif version == 2:
                async_sample_list = gen_async_sample_list_v2(async_sample_bytes)
            elif version == 3:
                async_sample_list = gen_async_sample_list_v3(async_sample_bytes)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            for async_sample in async_sample_list:
                start_time = async_sample.start_time
                cpu_time = async_sample.start_cpu_time
                stack_id = async_sample.stack_id
                alloc_size = async_sample.alloc_size
                alloc_count = async_sample.alloc_count

                async_sample = AsyncSample(
                    tid,
                    start_time,
                    cpu_time,
                    async_sample.target_tid,
                    async_sample.target_time,
                    stack_id,
                    alloc_size,
                    alloc_count,
                )
                tid_trace_list = async_sample_map.get(tid) or []
                tid_trace_list.append(async_sample)
                async_sample_map[tid] = tid_trace_list

    for tid, tid_trace_list in async_sample_map.items():

        for idx, async_sample in enumerate(tid_trace_list):
            stack_id = async_sample.stack_id
            stack = unwind_stack_from_callstack_table(callstack_table, stack_id)

            if len(stack) == 0 or stack[0] == 0:
                continue

            async_sample.calls = stack

    return async_sample_map


def read_dispatch_samples(
    con: sqlite3.Connection,
    trace_id: int,
    callstack_table: Dict[int, CallstackNode],
    version: int,
) -> Dict[int, List[DispatchSample]]:

    dispatch_sample_map: Dict[int, List[DispatchSample]] = {}

    if table_exists(con, dispatch_sample_table_name):

        def gen_dispatch_sample(row) -> DispatchSample:
            tid = row["source_tid"]
            source_time = int(row["source_time"])
            source_cpu_time = int(row["source_cpu_time"])
            target_tid = int(row["target_tid"])
            target_time = int(row["target_time"])
            stack_id = int(row["stack_id"])

            dispatch_sample = DispatchSample(
                tid,
                source_time,
                source_cpu_time,
                target_tid,
                target_time,
                stack_id,
                0,
                0,
            )

            return dispatch_sample

        def gen_dispatch_sample_v2(row) -> DispatchSample:
            tid = row["source_tid"]
            source_time = int(row["source_time"])
            source_cpu_time = int(row["source_cpu_time"])
            target_tid = int(row["target_tid"])
            target_time = int(row["target_time"])
            stack_id = int(row["stack_id"])
            alloc_size = int(row["alloc_size"])
            alloc_count = int(row["alloc_count"])

            dispatch_sample = DispatchSample(
                tid,
                source_time,
                source_cpu_time,
                target_tid,
                target_time,
                stack_id,
                alloc_size,
                alloc_count,
            )

            return dispatch_sample

        def gen_dispatch_sample_v3(row) -> DispatchSample:
            dispatch_sample = gen_dispatch_sample_v2(row)
            dispatch_sample.start_time *= 10
            dispatch_sample.target_time *= 10
            dispatch_sample.start_cpu_time *= 10

            return dispatch_sample
        
        def gen_dispatch_sample_v4(row) -> DispatchSample:
            tid = row["tid"]
            time = int(row["time"]) * 10
            cpu_time = int(row["cpu_time"]) * 10
            target_tid = int(row["target_tid"])
            target_time = int(row["target_time"]) * 10
            stack_id = int(row["stack_id"])
            alloc_size = int(row["alloc_size"])
            alloc_count = int(row["alloc_count"])

            dispatch_sample = DispatchSample(
                tid,
                time,
                cpu_time,
                target_tid,
                target_time,
                stack_id,
                alloc_size,
                alloc_count
            )

            return dispatch_sample

        dispatch_sample_sql = (
            f"select * from {dispatch_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(dispatch_sample_sql)

        for one in res.fetchall():
            dispatch_sample: Optional[DispatchSample] = None

            if version == 1:
                dispatch_sample = gen_dispatch_sample(one)
            elif version == 2:
                dispatch_sample = gen_dispatch_sample_v2(one)
            elif version == 3:
                dispatch_sample = gen_dispatch_sample_v3(one)
            elif version == 4:
                dispatch_sample = gen_dispatch_sample_v4(one)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            tid = dispatch_sample.tid
            tid_trace_list = dispatch_sample_map.get(tid) or []
            tid_trace_list.append(dispatch_sample)
            dispatch_sample_map[tid] = tid_trace_list

    if table_exists(con, dispatch_batch_sample_table_name):
        dispatch_sample_sql = f"select * from {dispatch_batch_sample_table_name} where trace_id={trace_id};"
        res = con.execute(dispatch_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            tid = one["tid"]
            dispatch_sample_bytes: bytes = one["nodes"]
            dispatch_sample_list: Optional[List[DispatchSampleNode]] = None

            if version == 1:
                dispatch_sample_list = gen_dispatch_sample_list(dispatch_sample_bytes)
            elif version == 2:
                dispatch_sample_list = gen_dispatch_sample_list_v2(
                    dispatch_sample_bytes
                )
            elif version == 3:
                dispatch_sample_list = gen_dispatch_sample_list_v3(
                    dispatch_sample_bytes
                )
            elif version == 4:
                dispatch_sample_list = gen_dispatch_sample_list_v4(
                    dispatch_sample_bytes
                )
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            for dispatch_sample in dispatch_sample_list:
                start_time = dispatch_sample.start_time
                cpu_time = dispatch_sample.start_cpu_time
                stack_id = dispatch_sample.stack_id
                alloc_size = dispatch_sample.alloc_size
                alloc_count = dispatch_sample.alloc_count

                dispatch_sample = DispatchSample(
                    tid,
                    start_time,
                    cpu_time,
                    dispatch_sample.target_tid,
                    dispatch_sample.target_time,
                    stack_id,
                    alloc_size,
                    alloc_count,
                )
                tid_trace_list = dispatch_sample_map.get(tid) or []
                tid_trace_list.append(dispatch_sample)
                dispatch_sample_map[tid] = tid_trace_list

    for tid, tid_trace_list in dispatch_sample_map.items():

        for idx, dispatch_sample in enumerate(tid_trace_list):
            stack_id = dispatch_sample.stack_id
            stack = unwind_stack_from_callstack_table(callstack_table, stack_id)

            if len(stack) == 0 or stack[0] == 0:
                continue

            dispatch_sample.calls = stack

    return dispatch_sample_map


def read_date_samples(
    con: sqlite3.Connection,
    trace_id: int,
    callstack_table: Dict[int, CallstackNode],
    version: int,
) -> Dict[int, List[DateSample]]:
    date_sample_map: Dict[int, List[DateSample]] = {}

    if table_exists(con, date_sample_table_name):
        # date sample
        def gen_date_sample(row) -> DateSample:
            tid = row["tid"]
            start_time = int(row["time"])
            cpu_time = int(row["cpu_time"])
            stack_id = int(row["stack_id"])

            date_sample = DateSample(tid, start_time, cpu_time, stack_id, 0, 0)

            return date_sample

        def gen_date_sample_v2(row) -> DateSample:
            tid = row["tid"]
            start_time = int(row["time"])
            cpu_time = int(row["cpu_time"])
            stack_id = int(row["stack_id"])
            alloc_size = int(row["alloc_size"])
            alloc_count = int(row["alloc_count"])

            date_sample = DateSample(
                tid, start_time, cpu_time, stack_id, alloc_size, alloc_count
            )

            return date_sample

        def gen_date_sample_v3(row) -> DateSample:
            date_sample = gen_date_sample_v2(row)
            date_sample.start_time *= 10
            date_sample.start_cpu_time *= 10

            return date_sample

        date_sample_sql = (
            f"select * from {date_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(date_sample_sql)

        for one in res.fetchall():
            date_sample: Optional[DateSample] = None

            if version == 1:
                date_sample = gen_date_sample(one)
            elif version == 2:
                date_sample = gen_date_sample_v2(one)
            elif version == 3:
                date_sample = gen_date_sample_v3(one)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            tid = date_sample.tid
            tid_trace_list = date_sample_map.get(tid) or []
            tid_trace_list.append(date_sample)
            date_sample_map[tid] = tid_trace_list

    if table_exists(con, date_batch_sample_table_name):
        date_sample_sql = (
            f"select * from {date_batch_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(date_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            tid = one["tid"]
            sample_bytes: bytes = one["nodes"]
            sample_list: Optional[List[DateSampleNode]] = None

            if version == 1:
                sample_list = gen_date_sample_list(sample_bytes)
            elif version == 2:
                sample_list = gen_date_sample_list_v2(sample_bytes)
            elif version == 3:
                sample_list = gen_date_sample_list_v3(sample_bytes)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            for date_sample in sample_list:
                start_time = date_sample.start_time
                cpu_time = date_sample.start_cpu_time
                stack_id = date_sample.stack_id
                alloc_size = date_sample.alloc_size
                alloc_count = date_sample.alloc_count

                date_sample = DateSample(
                    tid, start_time, cpu_time, stack_id, alloc_size, alloc_count
                )

                tid_trace_list = date_sample_map.get(tid) or []
                tid_trace_list.append(date_sample)
                date_sample_map[tid] = tid_trace_list

    for tid, tid_trace_list in date_sample_map.items():

        for idx, date_sample in enumerate(tid_trace_list):
            stack_id = date_sample.stack_id
            stack = unwind_stack_from_callstack_table(callstack_table, stack_id)

            if len(stack) == 0 or stack[0] == 0:
                continue

            date_sample.calls = stack

    return date_sample_map


def read_lock_samples(
    con: sqlite3.Connection,
    trace_id: int,
    callstack_table: Dict[int, CallstackNode],
    version: int,
) -> Dict[int, List[LockSample]]:
    lock_sample_map: Dict[int, List[LockSample]] = {}

    if table_exists(con, lock_sample_table_name):
        # lock sample
        lock_sample_sql = (
            f"select * from {lock_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(lock_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            sample = one["sample"]
            lock_sample: Optional[LockSample] = None

            if version == 1:
                lock_sample = gen_lock_sample(sample)
            elif version == 2:
                lock_sample = gen_lock_sample_v2(sample)
            elif version == 3:
                lock_sample = gen_lock_sample_v3(sample)
            elif version == 4:
                lock_sample = gen_lock_sample_v4(sample)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            tid = lock_sample.tid
            tid_trace_list = lock_sample_map.get(tid) or []
            tid_trace_list.append(lock_sample)
            lock_sample_map[tid] = tid_trace_list

    if table_exists(con, lock_batch_sample_table_name):
        lock_sample_sql = (
            f"select * from {lock_batch_sample_table_name} where trace_id={trace_id};"
        )
        res = con.execute(lock_sample_sql)

        for one in res.fetchall():
            trace_id = one["trace_id"]
            tid = one["tid"]
            sample_bytes: bytes = one["nodes"]
            sample_list: Optional[List[LockSampleNode]] = None

            if version == 1:
                sample_list = gen_lock_sample_list(sample_bytes)
            elif version == 2:
                sample_list = gen_lock_sample_list_v2(sample_bytes)
            elif version == 3:
                sample_list = gen_lock_sample_list_v3(sample_bytes)
            elif version == 4:
                sample_list = gen_lock_sample_list_v4(sample_bytes)
            else:
                raise RuntimeError(
                    "Unsupported version, please update 'btrace' command line tool!"
                )

            for lock_sample in sample_list:
                start_time = lock_sample.start_time
                cpu_time = lock_sample.start_cpu_time
                stack_id = lock_sample.stack_id
                alloc_size = lock_sample.alloc_size
                alloc_count = lock_sample.alloc_count

                lock_sample = LockSample(
                    tid,
                    lock_sample.id,
                    lock_sample.action,
                    start_time,
                    cpu_time,
                    stack_id,
                    alloc_size,
                    alloc_count,
                )
                tid_trace_list = lock_sample_map.get(tid) or []
                tid_trace_list.append(lock_sample)
                lock_sample_map[tid] = tid_trace_list

    for tid, tid_trace_list in lock_sample_map.items():

        for idx, lock_sample in enumerate(tid_trace_list):
            stack = []
            stack_id = lock_sample.stack_id

            stack = unwind_stack_from_callstack_table(callstack_table, stack_id)

            if len(stack) == 0 or stack[0] == 0:
                continue

            lock_sample.calls = stack

    return lock_sample_map


def read_callstack_table(
    con: sqlite3.Connection, trace_id: int, version: int
) -> Dict[int, CallstackNode]:
    callstack_table: Dict[int, CallstackNode] = {}

    callstack_record_sql = (
        f"select nodes from {callstack_table_name} where trace_id={trace_id};"
    )
    res = con.execute(callstack_record_sql)
    callstack_record = res.fetchone()
    callstack_table_bytes: bytes = callstack_record["nodes"]
    callstack_table: Dict[int, CallstackNode] = gen_callstack_map(callstack_table_bytes)

    return callstack_table


def read_thread_info(
    con: sqlite3.Connection, trace_id: int, version: int
) -> Dict[int, ThreadInfo]:
    thread_info_map: Dict[int, ThreadInfo] = {}

    if not table_exists(con, thread_info_table_name):
        return thread_info_map

    thread_info_record_sql = (
        f"select * from {thread_info_table_name} where trace_id={trace_id};"
    )
    res = con.execute(thread_info_record_sql)

    for one in res.fetchall():
        tid = one["tid"]
        name = one["name"]

        thread_info = ThreadInfo(tid, name)
        thread_info_map[tid] = thread_info

    return thread_info_map


def read_custome_perfetto_info(
    con: sqlite3.Connection, trace_id: int, version: int
) -> Tuple[Dict[str, List], Dict[str, List[PerfettoCounterInfo]]]:
    slice_info_map: Dict[str, List] = {}
    counter_info_map: Dict[str, List[PerfettoCounterInfo]] = {}

    if not table_exists(con, timeseries_info_table_name):
        return slice_info_map, counter_info_map

    thread_info_record_sql = (
        f"select * from {timeseries_info_table_name} where trace_id={trace_id};"
    )
    res = con.execute(thread_info_record_sql)

    for one in res.fetchall():
        tid: int = int(one["tid"])
        type: str = one["type"]
        timestamp: int = int(one["timestamp"])
        info_json: str = one["info"]

        info: Dict = json.loads(info_json)

        if not info.get("perfetto_type"):
            continue

        perfetto_type = info["perfetto_type"]

        if perfetto_type == "counter":
            val = float(info["val"])

            if type == "mem_usage":
                val = val / 1024 / 1024

            counter_info = PerfettoCounterInfo(type, timestamp, val)
            counter_info_list = counter_info_map.get(type) or []
            counter_info_list.append(counter_info)
            counter_info_map[type] = counter_info_list
        elif perfetto_type == "slice":
            begin_time = int(info["begin_time"])
            end_time = int(info["end_time"])

            if begin_time < 0 or end_time < 0 or end_time < begin_time:
                continue

            slice_info = PerfettoSliceInfo(type, begin_time, end_time)
            slice_info_list = slice_info_map.get(type) or []
            slice_info_list.append(slice_info)
            slice_info_map[type] = slice_info_list

    return slice_info_map, counter_info_map


def read_image_info(con: sqlite3.Connection, trace_id: int) -> List[ImageInfo]:
    result: List[ImageInfo] = []

    res = con.execute(
        f"select * from {image_info_table_name} where type=0 and trace_id={trace_id};"
    )

    for one in res.fetchall():
        address = one["address"]
        path = one["name"]
        uuid = one["uuid"]

        image_info = ImageInfo(address, path, uuid)

        result.append(image_info)

    result.sort(key=image_info_key)

    return result


def read_file(path: str) -> TraceRawContent:
    con = sqlite3.connect(path)
    con.row_factory = sqlite3.Row

    version = 1
    trace_id = 0
    trace_start_time = 0
    bundle_id = ""
    res = con.execute(f"select * from {btrace_table_name};")

    for one in res.fetchall():
        keys = one.keys()
        trace_id = one["trace_id"]
        trace_start_time = one["start_time"]

        if "version" in keys:
            version = one["version"]

        if "bundle_id" in keys:
            bundle_id = one["bundle_id"]

    callstack_table = read_callstack_table(con, trace_id, version)
    thread_info_map = read_thread_info(con, trace_id, version)
    slice_info_map, counter_info_map = read_custome_perfetto_info(
        con, trace_id, version
    )

    cpu_sample_map = read_cpu_samples(con, trace_id, callstack_table, version)
    mem_sample_map = read_mem_samples(con, trace_id, callstack_table, version)
    async_sample_map = read_async_samples(con, trace_id, callstack_table, version)
    dispatch_sample_map = read_dispatch_samples(con, trace_id, callstack_table, version)
    date_sample_map = read_date_samples(con, trace_id, callstack_table, version)
    lock_sample_map = read_lock_samples(con, trace_id, callstack_table, version)
    profile_sample_map = read_profile_samples(con, trace_id, callstack_table, version)
    time_range_sample_map = read_time_range_samples(con, trace_id, callstack_table, version)

    total_cpu_sample_list: List[CPUSample] = []
    sample_map: Dict[int, List[CPUSample]] = {}
    sorted_cpu_sample_map: Dict[int, sortedcontainers.SortedDict] = {}

    for tid, cpu_sample_list in cpu_sample_map.items():
        sample_map[tid] = cpu_sample_list

    for tid, date_sample_list in date_sample_map.items():
        sample_list = sample_map.get(tid, [])
        for date_sample in date_sample_list:
            cpu_sample = CPUSample(
                date_sample.tid,
                date_sample.start_time,
                date_sample.start_time,
                date_sample.start_cpu_time,
                date_sample.start_cpu_time,
                date_sample.stack_id,
                date_sample.alloc_size,
                date_sample.alloc_count,
                date_sample.calls,
            )
            cpu_sample.type = "date"
            sample_list.append(cpu_sample)
        sample_map[tid] = sample_list

    for tid, mem_sample_list in mem_sample_map.items():
        sample_list = sample_map.get(tid, [])
        for mem_sample in mem_sample_list:
            cpu_sample = CPUSample(
                mem_sample.tid,
                mem_sample.start_time,
                mem_sample.start_time,
                mem_sample.start_cpu_time,
                mem_sample.start_cpu_time,
                mem_sample.stack_id,
                mem_sample.alloc_size,
                mem_sample.alloc_count,
                mem_sample.calls,
            )
            cpu_sample.type = "mem"
            sample_list.append(cpu_sample)
        sample_map[tid] = sample_list

    for tid, async_sample_list in async_sample_map.items():
        sample_list = sample_map.get(tid, [])
        for async_sample in async_sample_list:
            cpu_sample = CPUSample(
                async_sample.tid,
                async_sample.start_time,
                async_sample.start_time,
                async_sample.start_cpu_time,
                async_sample.start_cpu_time,
                async_sample.stack_id,
                async_sample.alloc_size,
                async_sample.alloc_count,
                async_sample.calls,
            )
            cpu_sample.type = "async"
            sample_list.append(cpu_sample)
        sample_map[tid] = sample_list

    for tid, dispatch_sample_list in dispatch_sample_map.items():
        sample_list = sample_map.get(tid, [])
        for dispatch_sample in dispatch_sample_list:
            cpu_sample = CPUSample(
                dispatch_sample.tid,
                dispatch_sample.start_time,
                dispatch_sample.start_time,
                dispatch_sample.start_cpu_time,
                dispatch_sample.start_cpu_time,
                dispatch_sample.stack_id,
                dispatch_sample.alloc_size,
                dispatch_sample.alloc_count,
                dispatch_sample.calls,
            )
            cpu_sample.type = "dispatch"
            sample_list.append(cpu_sample)
        sample_map[tid] = sample_list

    for tid, lock_sample_list in lock_sample_map.items():
        sample_list = sample_map.get(tid, [])
        for lock_sample in lock_sample_list:
            cpu_sample = CPUSample(
                lock_sample.tid,
                lock_sample.start_time,
                lock_sample.start_time,
                lock_sample.start_cpu_time,
                lock_sample.start_cpu_time,
                lock_sample.stack_id,
                lock_sample.alloc_size,
                lock_sample.alloc_count,
                lock_sample.calls,
            )
            cpu_sample.type = "lock"
            sample_list.append(cpu_sample)
        sample_map[tid] = sample_list
        
    for tid, profile_sample_list in profile_sample_map.items():
        sample_list = sample_map.get(tid, [])
        for profile_sample in profile_sample_list:
            cpu_sample = CPUSample(
                profile_sample.tid,
                profile_sample.start_time,
                profile_sample.start_time,
                profile_sample.start_cpu_time,
                profile_sample.start_cpu_time,
                profile_sample.stack_id,
                profile_sample.alloc_size,
                profile_sample.alloc_count,
                profile_sample.calls,
            )
            cpu_sample.type = "profile"
            sample_list.append(cpu_sample)
        sample_map[tid] = sample_list
        
    for tid, time_range_sample_list in time_range_sample_map.items():
        sample_list = sample_map.get(tid, [])
        for time_range_sample in time_range_sample_list:
            cpu_sample = CPUSample(
                time_range_sample.tid,
                time_range_sample.start_time,
                time_range_sample.end_time,
                time_range_sample.start_cpu_time,
                time_range_sample.end_cpu_time,
                time_range_sample.stack_id,
                time_range_sample.alloc_size,
                time_range_sample.alloc_count,
                time_range_sample.calls,
            )
            cpu_sample.type = "time_range"
            sample_list.append(cpu_sample)
        sample_map[tid] = sample_list

    for tid, sample_list in sample_map.items():
        sample_list.sort(key=sample_info_key)

        thread_cpu_sample_map = sorted_cpu_sample_map.get(tid)

        if not thread_cpu_sample_map:
            thread_cpu_sample_map = sortedcontainers.SortedDict()

        for sample in sample_list:
            timestamp = sample.start_time
            idx = thread_cpu_sample_map.bisect_right(timestamp)

            if 0 < idx:
                start_time = thread_cpu_sample_map.keys()[idx - 1]
                val: CPUSample = thread_cpu_sample_map.values()[idx - 1]

                if start_time == timestamp:
                    diff_cpu_time = sample.start_cpu_time - val.start_cpu_time
                    sample.start_time += diff_cpu_time
                    sample.end_time += diff_cpu_time
                    timestamp = sample.start_time

                cpu_sample: CPUSample = thread_cpu_sample_map[start_time]

                if timestamp < cpu_sample.end_time:
                    # add new info(split cpu_sample)
                    new_start_time = sample.end_time + 1
                    new_start_cpu_time = sample.end_cpu_time
                    new_cpu_sample = CPUSample(
                        tid,
                        new_start_time,
                        cpu_sample.end_time,
                        new_start_cpu_time,
                        cpu_sample.end_cpu_time,
                        cpu_sample.stack_id,
                        cpu_sample.alloc_size,
                        cpu_sample.alloc_count,
                        cpu_sample.calls,
                    )
                    thread_cpu_sample_map[new_start_time] = new_cpu_sample

                    # modify existing item
                    cpu_sample.end_cpu_time = sample.start_cpu_time
                    cpu_sample.end_time = sample.start_time

            thread_cpu_sample_map[timestamp] = sample

        sorted_cpu_sample_map[tid] = thread_cpu_sample_map

    for tid, thread_cpu_sample_map in sorted_cpu_sample_map.items():
        for cpu_sample in thread_cpu_sample_map.values():
            total_cpu_sample_list.append(cpu_sample)

    image_info = read_image_info(con, trace_id)

    return TraceRawContent(
        bundle_id,
        trace_start_time,
        total_cpu_sample_list,
        image_info,
        thread_info_map,
        slice_info_map,
        counter_info_map,
    )
