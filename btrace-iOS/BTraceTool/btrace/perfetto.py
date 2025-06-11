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

import math
from typing import Dict, List, Union
from btrace.pb import perfetto_pb2
from btrace.model import CPUSample, MethodNode, MethodTree, PerfettoCounterInfo, PerfettoSliceInfo, SymbolInfo, TraceRawContent

        
PACKET_SEQ_ID = 1

class AutoIncrease:
    def __init__(self):
        self.val = 0
        
    def get(self):
        self.val += 1
        return self.val

    def reset(self):
        self.val = 0

def gen_root_packet(uuid: int) -> perfetto_pb2.TracePacket:
    child_ordering = perfetto_pb2.TrackDescriptor.ChildTracksOrdering.EXPLICIT
    root_track = perfetto_pb2.TrackDescriptor(uuid=uuid, name='Root', child_ordering=child_ordering)
    root_packet = perfetto_pb2.TracePacket()
    root_packet.track_descriptor.CopyFrom(root_track)

    return root_packet

def gen_process_packet(uuid: int, pid: int, process_name: str, parent_uuid: int) -> perfetto_pb2.TracePacket:
    process = perfetto_pb2.ProcessDescriptor(pid=pid, process_name=process_name)
    process_track = perfetto_pb2.TrackDescriptor(uuid=uuid, process=process, parent_uuid=parent_uuid)
    process_packet = perfetto_pb2.TracePacket()
    process_packet.track_descriptor.CopyFrom(process_track)

    return process_packet

def gen_sub_packet(uuid: int, parent_uuid: int, name: str) -> perfetto_pb2.TracePacket:
    sub_track = perfetto_pb2.TrackDescriptor(uuid=uuid, name=name, parent_uuid=parent_uuid)
    sub_packet = perfetto_pb2.TracePacket()
    sub_packet.track_descriptor.CopyFrom(sub_track)

    return sub_packet

def gen_thread_packet(uuid: int, pid: int, tid: int, thread_name: str) -> perfetto_pb2.TracePacket:
    thread = perfetto_pb2.ThreadDescriptor(pid=pid, tid=tid, thread_name=thread_name)
    thread_track = perfetto_pb2.TrackDescriptor(uuid=uuid, thread=thread)
    thread_packet = perfetto_pb2.TracePacket()
    thread_packet.track_descriptor.CopyFrom(thread_track)

    return thread_packet

def gen_counter_packet(uuid: int, parent_uuid: int, name: str) -> perfetto_pb2.TracePacket:
    counter = perfetto_pb2.CounterDescriptor()
    counter_track = perfetto_pb2.TrackDescriptor(uuid=uuid, parent_uuid=parent_uuid, name=name, counter=counter, sibling_order_rank=-1)
    counter_packet = perfetto_pb2.TracePacket()
    counter_packet.track_descriptor.CopyFrom(counter_track)

    return counter_packet

def gen_debug_annotation(k: str, v: object) -> perfetto_pb2.DebugAnnotation:
    annotation = perfetto_pb2.DebugAnnotation()
    annotation.name = k
    
    if isinstance(v, int):
        annotation.int_value = v
    elif isinstance(v, str):
        annotation.string_value = v

    return annotation

def gen_slice_event_packet(uuid: int, timestamp: int, name: str, event_type: perfetto_pb2.TrackEvent.Type, debug_infos: Dict=None) -> perfetto_pb2.TracePacket:
    track_event = perfetto_pb2.TrackEvent(type=event_type, track_uuid=uuid)

    debug_annotations: List[perfetto_pb2.DebugAnnotation] = []
    if debug_infos:
        for k, v in debug_infos.items():
            annotation = gen_debug_annotation(k, v)
            debug_annotations.append(annotation)

    track_event.debug_annotations.extend(debug_annotations)
    
    if event_type == perfetto_pb2.TrackEvent.Type.TYPE_SLICE_BEGIN:
        track_event.name = name

    event_packet = perfetto_pb2.TracePacket(timestamp=timestamp)
    event_packet.track_event.CopyFrom(track_event)
    event_packet.trusted_packet_sequence_id = PACKET_SEQ_ID
    
    return event_packet

def gen_counter_event_packet(uuid: int, timestamp: int, counter_value: Union[int, float], debug_infos: Dict=None) -> perfetto_pb2.TracePacket:
    event_type = perfetto_pb2.TrackEvent.Type.TYPE_COUNTER
    track_event = perfetto_pb2.TrackEvent(type=event_type, track_uuid=uuid)
    
    if isinstance(counter_value, int):
        track_event.counter_value = counter_value
    elif isinstance(counter_value, float):
        track_event.double_counter_value = counter_value

    debug_annotations: List[perfetto_pb2.DebugAnnotation] = []
    if debug_infos:
        for k, v in debug_infos.items():
            annotation = gen_debug_annotation(k, v)
            debug_annotations.append(annotation)

    track_event.debug_annotations.extend(debug_annotations)

    event_packet = perfetto_pb2.TracePacket(timestamp=timestamp)
    event_packet.track_event.CopyFrom(track_event)
    event_packet.trusted_packet_sequence_id = PACKET_SEQ_ID
    
    return event_packet


def method_tree_to_trace(trace_packet_list: List[perfetto_pb2.TracePacket], thread_track: perfetto_pb2.TrackDescriptor, method_tree: MethodTree, method_node: MethodNode, addr_symbol_map: Dict[int, SymbolInfo]):
    symbol_info = addr_symbol_map[method_node.address]
    symbol_name = symbol_info.name

    start_time = (method_node.start_time+method_tree.start_time) * 1000
    cpu_time = method_node.end_cpu_time - method_node.start_cpu_time
    cpu_time_s = math.floor(cpu_time / 1_000_000)
    cpu_time_ms = math.floor((cpu_time - cpu_time_s * 1_000_000)/ 1000)
    cpu_time_us = cpu_time % 1000
    cpu_time_str = ""
    alloc_size = method_node.end_alloc_size - method_node.start_alloc_size
    alloc_count = method_node.end_alloc_count - method_node.start_alloc_count
    
    if cpu_time_us:
        cpu_time_str = f"{cpu_time_us:3d}us"
    if cpu_time_ms:
        cpu_time_str = f"{cpu_time_ms}ms " + cpu_time_str
    if cpu_time_s:
        cpu_time_str = f"{cpu_time_s}s " + cpu_time_str

    debug_infos = {
        "image": symbol_info.image,
        "file": symbol_info.file,
        "cpu_time": cpu_time_str,
        "cpu_time_int": cpu_time,
        "alloc_size": alloc_size,
        "alloc_count": alloc_count
    }
    start_packet = gen_slice_event_packet(thread_track.uuid, start_time, symbol_name, perfetto_pb2.TrackEvent.Type.TYPE_SLICE_BEGIN, debug_infos)
    trace_packet_list.append(start_packet)
    
    for child in method_node.children:
        method_tree_to_trace(trace_packet_list, thread_track, method_tree, child, addr_symbol_map)
    
    end_time = (method_node.end_time+method_tree.start_time) * 1000
    end_packet = gen_slice_event_packet(thread_track.uuid, end_time, symbol_name, perfetto_pb2.TrackEvent.Type.TYPE_SLICE_END)
    trace_packet_list.append(end_packet)

def gen_perfetto_trace(method_tree_list: List[MethodTree], 
                       addr_symbol_map: Dict[int, SymbolInfo],
                       raw_content: TraceRawContent) -> perfetto_pb2.Trace:

    trace = perfetto_pb2.Trace()
    trace_packet_list: List[perfetto_pb2.TracePacket] = []

    auto_increase = AutoIncrease()
    
    root_packet = gen_root_packet(auto_increase.get())
    trace_packet_list.append(root_packet)
    root_uuid = root_packet.track_descriptor.uuid

    bundle_id = raw_content.bundle_id or "Aweme"
    process_packet = gen_process_packet(auto_increase.get(), 1, bundle_id, root_uuid)
    trace_packet_list.append(process_packet)
    process = process_packet.track_descriptor.process
     
    slice_info_map = raw_content.slice_info_map
    counter_info_map = raw_content.counter_info_map
        
    for counter_name, v in counter_info_map.items():
        counter_info_list: List[PerfettoCounterInfo] = v
        counter_packet = gen_counter_packet(auto_increase.get(), root_uuid, f"{counter_name}")
        trace_packet_list.append(counter_packet)
        
        for counter_info in counter_info_list:
            counter_track = counter_packet.track_descriptor
            time = (counter_info.start_time - raw_content.start_time) * 1000
            val = counter_info.val
            counter_event = gen_counter_event_packet(counter_track.uuid, time, val)
            trace_packet_list.append(counter_event)
            
    for slice_name, v in slice_info_map.items():
        slice_info_list: List[PerfettoSliceInfo] = v
        slice_packet = gen_sub_packet(auto_increase.get(), root_uuid, f"{slice_name}")
        trace_packet_list.append(slice_packet)
        
        for slice_info in slice_info_list:
            slice_track = slice_packet.track_descriptor
            begin_time = slice_info.begin_time * 1000
            start_packet = gen_slice_event_packet(slice_track.uuid, begin_time, slice_name, perfetto_pb2.TrackEvent.Type.TYPE_SLICE_BEGIN)
            trace_packet_list.append(start_packet)
            end_time = slice_info.end_time * 1000
            end_packet = gen_slice_event_packet(slice_track.uuid, end_time, slice_name, perfetto_pb2.TrackEvent.Type.TYPE_SLICE_END)
            trace_packet_list.append(end_packet)

    thread_info_map = raw_content.thread_info_map
    
    for method_tree in method_tree_list:
        tid = method_tree.tid
        thread_info = thread_info_map.get(tid)
        name = ""
        
        if method_tree.main_thread:
            name = "main"
        elif thread_info:
            name = thread_info.name

        full_name = f"Callstack: Thread {name}"
        thread_packet = gen_thread_packet(auto_increase.get(), process.pid, tid, full_name)
        trace_packet_list.append(thread_packet)
        thread_track = thread_packet.track_descriptor
  
        for method_node in method_tree.root.children:
            method_tree_to_trace(trace_packet_list, thread_track, method_tree, method_node, addr_symbol_map)

    trace.packet.extend(trace_packet_list)

    return trace
