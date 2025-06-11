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

import os
import time
import subprocess
from typing import Dict, List
from btrace.model import ImageInfo, SymbolInfo
from btrace.util import macho_uuid

main_symbol_name_set = set(["main"])

def get_sys_symbol_path(device_info: Dict):
    home_dir = os.path.expanduser("~")
    device_type = device_info["device_type"]
    os_version = device_info["os_version"]
    build_version = device_info["build_version"]
    device_name = f"{device_type} {os_version} ({build_version})"
    return os.path.join(home_dir, f"Library/Developer/Xcode/iOS DeviceSupport/{device_name}/Symbols")

def get_image_dsym_path(dsym_path: str, image_info_map: Dict[str, ImageInfo]) -> Dict[str, str]:
    image_dsym_path_map: Dict[str, str] = {}

    if not dsym_path:
        return image_dsym_path_map

    for file_dir, dirs, files in os.walk(dsym_path):
        for file_name in files:
            if not image_info_map.get(file_name):
                continue
            
            file_path = os.path.join(file_dir, file_name)
            uuid = macho_uuid(file_path)
            if uuid:
                if image_dsym_path_map.get(uuid):
                    continue
                image_dsym_path_map[uuid] = file_path
    

    return image_dsym_path_map

def symbolicate(image_dsym_path_map: Dict[str, str], image_info: ImageInfo, addr_list: List[int], addr_symbol_map: Dict[int, SymbolInfo]):
    image_dsym_path = image_dsym_path_map.get(image_info.uuid)
    
    t1 = time.time()
    
    if image_dsym_path:
        atos(image_dsym_path, image_info, addr_list, addr_symbol_map)
    # else:
    #     retrace(image_info, addr_list, addr_symbol_map)

    t2 = time.time()
    t = t2 - t1
    print(f"> symbolicate {image_info.name}, time: {t:.2f}s")

# def retrace(image_info: ImageInfo, addr_list: List[int], addr_symbol_map: Dict[int, SymbolInfo]):
#     request = RetraceIOSSymbolRequest{
#                     ImageAddress: int64(imageInfo.Address),
#                     ImageName:    image_info.name,
#                     ImageUuid:    image_info.uuid,
#                     Addresses:    addressList,
#                 }

def atos(image_dsym_path, image_info: ImageInfo, address_list: List[int], result_map: Dict[int, SymbolInfo]):
    ret_list : List[str] = []

    image_load_addr = image_info.address
    address_str = " ".join([hex(i) for i in address_list])
    cmd_str = f"atos -o '{image_dsym_path}' -l {hex(image_load_addr)} {address_str}"
    # print("cmd_str: ", cmd_str)
    res_list = subprocess.run(cmd_str, shell=True, capture_output=True).stdout.decode().split("\n")
    ret_list.extend(res_list[:-1])


    for idx, full_symbol in enumerate(ret_list):
        func = ""
        image = ""
        file = ""
        
        symbol_list = full_symbol.split(" (in ")
        func = symbol_list[0]
        
        if func == "_start_wqthread" or \
            func == "_thread_start" or \
            func == "start_wqthread" or \
            func == "thread_start":
            continue
        
        if len(symbol_list) >= 2:
            image_file_name_list = symbol_list[1].split(")")
            
            if len(image_file_name_list) == 2:
                image = image_file_name_list[0]
            elif len(image_file_name_list) == 3:
                image = image_file_name_list[0]
                file = image_file_name_list[1].strip()[1:]

        address = address_list[idx]
        result_map[address] = SymbolInfo(address, func, image, file, image_info.is_sys)