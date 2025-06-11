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
import tempfile
from typing import Dict, List, Optional

from btrace import util

def get_device_list():
    """
    device info:
    {
        'Identifier': '00008030-000D08E922B8802E',
        'DeviceName': 'iPhone',
        'BuildVersion': '21G93',
        'ProductVersion': '17.6.1',
        'ProductType': 'iPhone12,1'
    }
    """
    connected_devices = []
    device_info_list: List[Dict] = []
    
    with tempfile.NamedTemporaryFile() as fp:
        list_device_cmd = f"xcrun devicectl list devices -j {fp.name}"
        util.sync_run(list_device_cmd)
        ret: Dict = json.load(fp)
        result = ret.get("result") or {}
        device_info_list = result.get("devices") or []
        
    for device_info in device_info_list:
        if device_info["connectionProperties"]["tunnelState"] != "connected":
            continue
        
        device_properties = device_info["deviceProperties"]
        hardwareProperties = device_info["hardwareProperties"]
        
        connected_devices.append({
            'Identifier': hardwareProperties["udid"],
            'DeviceName': device_properties["name"],
            'BuildVersion': device_properties["osBuildUpdate"],
            'ProductVersion': device_properties["osVersionNumber"],
            'ProductType': hardwareProperties["productType"]
        })

    return connected_devices

def connect_if_need(device_id) -> int:
    ret = util.sync_run(f"lsof -i:{util.SRC_PORT} | grep {util.SRC_PORT}", shell=True)

    if ret != "":
        prev_pid = int(ret.split()[1])
        util.kill_process(prev_pid)

    pid = forward_port(device_id, util.SRC_PORT, util.DST_PORT)

    return pid


def forward_port(device_id: str, src: int, dst: int) -> int:
    forward_cmd = [
        "iproxy",
        "-u",
        device_id,
        str(src),
        str(dst),
    ]
    # print(" ".join(forward_cmd))
    pid = util.async_run(forward_cmd)
    
    return pid