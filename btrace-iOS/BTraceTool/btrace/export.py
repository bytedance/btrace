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
import os
import sqlite3
from typing import Dict, List

from btrace.extractor import read_file
from btrace.model import MethodNode, MethodTree, SymbolInfo
from btrace.parse import parse_raw_data


def serialize_tree_list(method_tree_list: List[MethodTree]) -> List[Dict]:
    result: List[Dict] = []

    for method_tree in method_tree_list:
        depth = -1
        node_id = -1
        tid = method_tree.tid
        node_list = method_tree.root.children

        while node_list:
            depth += 1
            next_node_list: List[MethodNode] = []

            for node in node_list:
                children = node.children

                if children:
                    next_node_list.extend(children)

                node_id += 1
                node.parent_id = node.parent.node_id
                node.node_id = node_id
                cpu_time = node.end_cpu_time - node.start_cpu_time
                
                result.append({
                    "tid": tid,
                    "depth": depth,
                    "node_id": node.node_id,
                    "parent_id": node.parent_id,
                    "address": node.address,
                    "begin_time": node.start_time,
                    "end_time": node.end_time,
                    "cpu_time": cpu_time,
                    "alloc_size": node.alloc_size,
                    "alloc_count": node.alloc_count
                })

            node_list = next_node_list

    return result
 
 
def serialize_symbol_map(addr_symbol_map: Dict[int, SymbolInfo]) -> List[Dict]:
    result: List[Dict] = []
    
    for addr, symbol in addr_symbol_map.items():
        result.append({
            "address": addr,
            "name": symbol.name,
            "image": symbol.image,
            "file": symbol.file,
            "is_sys": symbol.is_sys
        })

    return result


def write_to_sqlite(db_path: str, method_node_list: List[Dict], symbol_info_list: List[Dict]):
    con = sqlite3.connect(db_path)
    cur = con.cursor()
    
    method_info_table = "method_info"
    cur.execute(f"CREATE TABLE if not exists {method_info_table}(tid INTEGER, depth INTEGER,"
                "node_id INTEGER, parent_id INTEGER, address INTEGER, begin_time INTEGER,"
                "end_time INTEGER, cpu_time INTEGER, alloc_size INTEGER, alloc_count INTEGER)")
    method_value_list = []
    for node in method_node_list:
        method_value_list.append((node["tid"], node["depth"], node["node_id"], node["parent_id"],
                                  node["address"], node["begin_time"], node["end_time"],
                                  node["cpu_time"], node["alloc_size"], node["alloc_count"]))
    
    cur.executemany(f"INSERT INTO {method_info_table} VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    method_value_list)
    con.commit()
    
    symbol_info_table = "symbol_info"
    cur.execute(f"CREATE TABLE if not exists {symbol_info_table}(address INTEGER, name TEXT, image TEXT,"
                "file TEXT, is_sys INTEGER)")
    symbol_value_list = []
    for info in symbol_info_list:
        symbol_value_list.append((info["address"], info["name"], info["image"], info["file"],
                                  info["is_sys"]))
    
    cur.executemany(f"INSERT INTO {symbol_info_table} VALUES(?, ?, ?, ?, ?)",
                    symbol_value_list)
    con.commit()


def export(file_path: str, dsym_path: str, output_path: str, export_type: str="sqlite", force: bool=False, sys_symbol: bool=False):
    print("exporting data, please wait...")
    
    if export_type not in ["sqlite", "json"]:
        print("error export type!")
        return
    
    if not os.path.exists(dsym_path):
        err_msg = "Dsym path does not exist!"
        raise RuntimeError(err_msg)
    
    base_name = os.path.basename(file_path)
    suffix = "_export." + export_type
    
    if not output_path:
        file_prefix, _ = os.path.splitext(file_path)
        output_path = file_prefix + suffix
    else:
        file_prefix, _ = os.path.splitext(base_name)
        output_path = os.path.join(output_path, file_prefix+suffix)

    device_info_list = base_name.split("_")
    device_info = {}
    
    if len(device_info_list) == 4:
        device_info = {
            "device_type": device_info_list[0],
            "os_version": device_info_list[1],
            "build_version": device_info_list[2]
        }

    file_exists = os.path.exists(output_path)

    if not force and file_exists:
        print("using cached result, add the '-f' parameter to force re-parsing.")
        return
    else:
        if file_exists:
            os.remove(output_path)

        raw_content = read_file(file_path)
        method_tree_list, addr_symbol_map = parse_raw_data(
            raw_content, dsym_path, device_info, sys_symbol
        )
        
        method_node_list = serialize_tree_list(method_tree_list)
        symbol_info_list = serialize_symbol_map(addr_symbol_map)
        
        if export_type == "sqlite":
            write_to_sqlite(output_path, method_node_list, symbol_info_list)
        elif export_type == "json":       
            result_json_map: Dict[str, Dict] = {}
            result_json_map["method_info"] = method_node_list
            result_json_map["symbol_info"] = symbol_info_list
            
            with open(output_path, "w") as fp:
                json.dump(result_json_map, fp)