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

import bisect
import os
from typing import Dict, List, Set, Optional, Tuple
from btrace import symbol_info
from btrace.extractor import read_file
from btrace.model import (
    CPUSample,
    ImageInfo,
    MethodNode,
    MethodTree,
    SymbolInfo,
    TraceRawContent,
    image_info_key,
    sample_info_key,
)
from btrace.open_trace import open_trace
from btrace.pb import perfetto_pb2
from btrace.perfetto import gen_perfetto_trace


def get_image_info(image_address_list: List[int], address: int) -> int:
    idx = bisect.bisect(image_address_list, address)

    return idx - 1


def symbolicate(
    dsym_path: str,
    device_info: Dict,
    sys_symbol: bool,
    image_info_map: Dict[str, ImageInfo],
    image_addr_set_map: Dict[str, Set[int]],
) -> Dict[int, SymbolInfo]:
    addr_symbol_map: Dict[int, SymbolInfo] = {}

    image_dsym_path_map = {}
    app_image_dsym_path_map = symbol_info.get_image_dsym_path(dsym_path, image_info_map)
    image_dsym_path_map.update(app_image_dsym_path_map)
    if sys_symbol:
        sys_symbol_path = symbol_info.get_sys_symbol_path(device_info)
        sys_image_dsym_path_map = symbol_info.get_image_dsym_path(
            sys_symbol_path, image_info_map
        )
        image_dsym_path_map.update(sys_image_dsym_path_map)

    print("> gen symbol...")

    for image_name, address_set in image_addr_set_map.items():
        address_list: List[int] = []
        image_info = image_info_map[image_name]
        if image_info.is_sys:
            if not sys_symbol:
                continue

        for address in address_set:
            address_list.append(address)

            if 20_000 <= len(address_list):
                symbol_info.symbolicate(
                    image_dsym_path_map, image_info, address_list, addr_symbol_map
                )
                address_list = []

        if 0 < len(address_list):
            symbol_info.symbolicate(
                image_dsym_path_map, image_info, address_list, addr_symbol_map
            )

    return addr_symbol_map


def beautify_method_tree(root: MethodNode):
    if not root:
        return

    children = root.children
    new_children: List[MethodNode] = []

    for idx, node in enumerate(children):
        if idx == len(children) - 1:
            next_time = max(root.end_time, node.end_time)
        else:
            next_node = children[idx + 1]
            next_time = next_node.start_time

        diff_time = next_time - node.end_time
        node.end_time = node.end_time + int(min(diff_time, 1000))  # 1000us
        child_wall_time = node.end_time - node.start_time

        if child_wall_time <= 0:
            continue

        new_children.append(node)

        if node.end_cpu_time < node.start_cpu_time:
            node.end_cpu_time = node.start_cpu_time

        beautify_method_tree(node)

    root.children = new_children

    return


def construct_method_tree(
    tid: int,
    cpu_sample_list: List[CPUSample],
    addr_symbol_map: Dict[int, SymbolInfo],
    main_thread: bool,
):
    cpu_sample_list.sort(key=sample_info_key)

    method_tree = MethodTree(tid, 0, main_thread)

    for idx, sample in enumerate(cpu_sample_list):
        tid = sample.tid
        start_time = sample.start_time
        end_time = sample.end_time
        start_cpu_time = sample.start_cpu_time
        end_cpu_time = sample.end_cpu_time
        alloc_size = sample.alloc_size
        alloc_count = sample.alloc_count
        stack = sample.calls

        if idx == 0:
            parent_node = method_tree.root

            for i, addr in enumerate(stack):
                symbol = addr_symbol_map[addr]
                node = MethodNode(
                    addr,
                    start_time,
                    end_time,
                    start_cpu_time,
                    end_cpu_time,
                    alloc_size,
                    alloc_size,
                    alloc_count,
                    alloc_count,
                )
                method_tree.put_child(node, parent_node)
                parent_node = node
        else:
            i = 0
            j = 0
            prev_sample = cpu_sample_list[idx - 1]
            prev_stack = prev_sample.calls
            parent_node = method_tree.root

            while i <= len(prev_stack) - 1 and j <= len(stack) - 1:
                children = parent_node.children
                last_child = children[-1]

                prev_addr = prev_stack[i]
                addr = stack[j]

                prev_symbol: SymbolInfo = addr_symbol_map[prev_addr]
                symbol: SymbolInfo = addr_symbol_map[addr]

                if not prev_symbol.is_same(symbol):
                    break
                else:
                    last_child.update_cpu_time(end_cpu_time)
                    last_child.update_end_time(end_time)
                    last_child.update_alloc_size(alloc_size)
                    last_child.update_alloc_count(alloc_count)

                parent_node = last_child
                i += 1
                j += 1

            while j <= len(stack) - 1:
                addr = stack[j]
                symbol = addr_symbol_map[addr]
                node = MethodNode(
                    addr,
                    start_time,
                    end_time,
                    start_cpu_time,
                    end_cpu_time,
                    prev_sample.alloc_size,
                    alloc_size,
                    prev_sample.alloc_count,
                    alloc_count,
                )
                method_tree.put_child(node, parent_node)
                parent_node = node
                j += 1

    if 0 < len(method_tree.root.children):
        firstChild = method_tree.root.children[0]
        lastChild = method_tree.root.children[-1]
        method_tree.root.start_time = firstChild.start_time
        method_tree.root.start_cpu_time = firstChild.start_cpu_time
        method_tree.root.end_time = lastChild.end_time
        method_tree.root.end_cpu_time = lastChild.end_cpu_time
 
    beautify_method_tree(method_tree.root)

    return method_tree


def parse_raw_data(
    raw_content: TraceRawContent,
    dsym_path: str,
    device_info: Dict,
    sys_symbol: bool = False,
) -> Tuple[List[MethodTree], Dict[int, SymbolInfo]]:
    cpu_sample_list = raw_content.cpu_sample_list
    image_info_list = raw_content.image_info_list

    image_info_map: Dict[ImageInfo] = {}
    image_addr_set_map: Dict[str, Set[int]] = {}
    image_address_list = [image_info.address for image_info in image_info_list]

    for sample in cpu_sample_list:
        for address in sample.calls:
            image_info_idx = get_image_info(image_address_list, address)

            if image_info_idx < 0:
                continue

            image_info = image_info_list[image_info_idx]
            image_name = image_info.name

            if not image_info_map.get(image_name):
                image_info_map[image_name] = image_info

            address_set = image_addr_set_map.get(image_name)

            if not address_set:
                address_set = set()

            if address not in address_set:
                address_set.add(address)

            image_addr_set_map[image_name] = address_set

    addr_symbol_map = symbolicate(
        dsym_path, device_info, sys_symbol, image_info_map, image_addr_set_map
    )

    method_tree_list: List[MethodTree] = []
    thread_sample_list_map: Dict[int, List[CPUSample]] = {}

    main_tid = 0xFFFFFFFF
    for sample in cpu_sample_list:

        if len(sample.calls) <= 0:
            continue

        stack: List[int] = []
        tid = sample.tid

        if tid < main_tid:
            main_tid = tid

        for address in sample.calls:
            symbol_info = addr_symbol_map.get(address)

            if symbol_info:
                stack.append(address)

        if len(stack) == 0:
            continue

        sample.calls = stack
        thread_sample_list = thread_sample_list_map.get(tid, [])
        thread_sample_list.append(sample)
        thread_sample_list_map[tid] = thread_sample_list

    for tid, cpu_sample_list in thread_sample_list_map.items():
        main_thread = False

        if main_tid == tid:
            main_thread = True

        method_tree = construct_method_tree(
            tid, cpu_sample_list, addr_symbol_map, main_thread
        )

        duration = method_tree.root.end_time - method_tree.root.start_time
        cpu_duration = method_tree.root.end_cpu_time - method_tree.root.start_cpu_time

        if duration < 0 or cpu_duration < 0 or duration < cpu_duration:
            continue
        method_tree_list.append(method_tree)

    return method_tree_list, addr_symbol_map


def parse(
    file_path: str, dsym_path: str, force: bool = False, sys_symbol: bool = False
):
    print("parsing file, please wait...")

    if not os.path.exists(dsym_path):
        err_msg = "Dsym path does not exist!"
        raise RuntimeError(err_msg)

    file_prefix, file_ext = os.path.splitext(file_path)
    output_path = file_prefix + ".pb"

    base_name = os.path.basename(file_path)
    device_info_list = base_name.split("_")
    device_info = {}

    if len(device_info_list) == 4:
        device_info = {
            "device_type": device_info_list[0],
            "os_version": device_info_list[1],
            "build_version": device_info_list[2],
        }

    if not force and os.path.exists(output_path):
        print("using cached result, add the '-f' parameter to force re-parsing.")
    else:
        raw_content = read_file(file_path)
        method_tree_list, addr_symbol_map = parse_raw_data(
            raw_content, dsym_path, device_info, sys_symbol
        )
        trace_pb = gen_perfetto_trace(method_tree_list, addr_symbol_map, raw_content)

        with open(output_path, "wb") as fp:
            trace_bytes = trace_pb.SerializeToString()
            fp.write(trace_bytes)

    open_trace(output_path)
