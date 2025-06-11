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

import gzip
import json
import time
import tempfile
from typing import Dict, List, Optional
import requests

from btrace import util


default_bundle_id_list = [
    "org.cocoapods.demo.BTrace-Example", # for debug
]

def get_supported_app_list(device_id: str, bundle_id_list=None):
    """
    app info:
    {
        "CFBundleIdentifier": "com.ss.iphone.ugc.Aweme",
        "CFBundleDisplayName": "抖音",
        "CFBundleExecutable": "Aweme",
    }
    """
    installed_apps = []
    app_info_list: List[Dict] = []
    
    if not bundle_id_list or len(bundle_id_list) == 0:
        bundle_id_list = default_bundle_id_list
    
    with tempfile.NamedTemporaryFile() as fp:
        list_app_cmd = f"xcrun devicectl device info apps -d {device_id} -j {fp.name}"
        util.sync_run(list_app_cmd)
        ret: Dict = json.load(fp)
        result = ret.get("result") or {}
        app_info_list = result.get("apps") or []
    
    for app_info in app_info_list:
        bundle_id = app_info["bundleIdentifier"]
        name = app_info["name"]
        
        if bundle_id in bundle_id_list:
            installed_apps.append({
                "CFBundleIdentifier": bundle_id,
                "CFBundleDisplayName": name,
            })
        # print(json.dumps(v))
    
    return installed_apps
    
def launch(device_id: str, bundle_id: str, config: Optional[Dict]=None) -> int:
    
    if not config:
        config = util.DEFAULT_CONFIG

    env_key = "BTrace"
    env_val = json.dumps(config)
    env_dict = {env_key: env_val}
    env_cmd_str = json.dumps(env_dict)
    
    if util.verbose():
        print("launch config: ", env_val)

    launch_process_cmd = f"xcrun devicectl device process launch --terminate-existing -e '{env_cmd_str}'" + \
                         f" -d {device_id} {bundle_id}"
    util.sync_run(launch_process_cmd)
        # print(f'Process launched with pid {pid}')
        
    time.sleep(1)

def start_record():
    url = util.API_HOST + "/record/start"
    
    config = util.DEFAULT_CONFIG
    
    if util.verbose():
        print("record config: ", json.dumps(config))
        
    resp = requests.post(url, json=config, timeout=1)
    resp.raise_for_status()

def stop_record() -> Optional[bytes]:
    url = util.API_HOST + "/record/stop"
    
    config = {}
    resp = requests.post(url, json=config, timeout=30)
    resp.raise_for_status()
    
    gz_data = resp.content
    
    if not gz_data:
      return None
    
    return gzip.decompress(gz_data)

def hello():
    url = util.API_HOST + "/hello"

    resp = requests.get(url, timeout=1)
    resp.raise_for_status()


def check_connection():
    cnt = 0
    err = None
    
    while cnt < 10:
        cnt += 1
        
        try:
            hello()
        except Exception as e:
            err = e
        else:
            err = None
        
        if not err:
            break

        time.sleep(0.3)
        
    if err:
        print(err)
        raise RuntimeError("Failed to forward port! Please make sure that the app is running in the foreground.")


def upload_config():
    url = util.API_HOST + "/config/upload"
    
    config = util.DEFAULT_CONFIG
    
    resp = requests.post(url, json=config, timeout=3)
    resp.raise_for_status()