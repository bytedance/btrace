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
import sys
import signal
import subprocess
from typing import Dict, List, Optional, Union


SRC_PORT = 11288
DST_PORT = 1128
API_HOST = "http://localhost:" + str(SRC_PORT)
BTRACE_ROOT_DIR = os.path.dirname(os.path.abspath(__file__))

TIMESERIES_CONFIG = {}
TIMEPROFILER_CONFIG = {"period": 1, "bg_period": 2, "active_only": False, "merge_stack": False}
MEMPROFILER_CONFIG = {"lite_mode": 0, "interval": 10, "period": 1, "bg_period": 2}
DATEPROFILER_CONFIG = {"period": 1, "bg_period": 2}
DISPATCHPROFILER_CONFIG = {"period": 1, "bg_period": 2}
LOCKPROFILER_CONFIG = {"period": 1, "bg_period": 2}

DEFAULT_CONFIG = {
    "enable": True,
    "timeout": 36000,
    "max_records": 10000,
    "buffer_size": 4,
    "plugins": [
        {"name": "timeseries", "config": TIMESERIES_CONFIG},
        {"name": "timeprofiler", "config": TIMEPROFILER_CONFIG},
        {"name": "memoryprofiler", "config": MEMPROFILER_CONFIG},
        {"name": "dateprofiler", "config": DATEPROFILER_CONFIG},
        {"name": "dispatchprofiler", "config": DISPATCHPROFILER_CONFIG},
        {"name": "lockprofiler", "config": LOCKPROFILER_CONFIG}
    ]
}

VERBOSE = False

def set_record_parameter(main_only=False, hitch_thres=50):
    plugins: List[Dict] = DEFAULT_CONFIG["plugins"]
            
    DEFAULT_CONFIG["main_only"] = main_only

    if main_only:
        TIMEPROFILER_CONFIG["bg_period"] = 10_000_000
        MEMPROFILER_CONFIG["bg_period"] = 10_000_000
        DATEPROFILER_CONFIG["bg_period"] = 10_000_000
        DISPATCHPROFILER_CONFIG["bg_period"] = 10_000_000
        LOCKPROFILER_CONFIG["bg_period"] = 10_000_000
    
    MONITOR_CONFIG = {"hang": {"config": {"hitch": hitch_thres}}}
    MONITOR_PLUGIN = {"name": "monitor", "config": MONITOR_CONFIG}
    plugins.append(MONITOR_PLUGIN)


def sync_run(cmd: Union[List, str], shell=True) -> str:
    cmd_str = cmd

    if isinstance(cmd_str, list):
        cmd_str = " ".join([str(x) for x in cmd_str])
        
    if verbose():
        print(f"cmd: {cmd_str}")

    ret = subprocess.run(cmd_str, shell=shell, capture_output=True)
    err = ret.stderr.decode()
    out = ret.stdout.decode()
    
    if not out and err:
        err_msg = f"err: {err}"
        raise RuntimeError(err_msg)

    return out


def async_run(cmd: List[str], silent=True) -> int:
    stdout = None
    stderr = None
    
    if verbose():
        cmd_str = " ".join([str(x) for x in cmd])
        print(f"cmd: {cmd_str}")

    if silent and not verbose():
        stdout = subprocess.DEVNULL
        stderr = subprocess.DEVNULL

    p = subprocess.Popen(cmd, stdout=stdout, stderr=stderr, start_new_session=True)

    return p.pid


def get_git_email() -> str:
    ret = sync_run("git config --global user.email")
    return ret


def kill_process(pid: int):
    if verbose():
        print(f"kill pid: {pid}")
    os.kill(pid, signal.SIGKILL)


def clean_subprocess(pid_list: List[int]):
    for pid in pid_list:
        kill_process(pid)


def macho_uuid(file_path) -> Optional[str]:
    uuid = ""
    
    from macholib import MachO, mach_o
    import lief
    if not lief.is_macho(file_path):
        return uuid

    # fat_bin = lief.MachO.parse(file_path)
    
    # if not fat_bin:
    #     return uuid
    
    # arm64_bin: Optional[lief.MachO.Binary] = None
    
    # for i in range(fat_bin.size):
    #     macho_bin = fat_bin.at(i)
    #     if macho_bin.header.cpu_type == lief.MachO.Header.CPU_TYPE.ARM64:
    #         arm64_bin = macho_bin
    #         break
    
    # if not arm64_bin:
    #     return uuid
    
    # if arm64_bin.has_uuid:
    #     uuid_list: List[int] = arm64_bin.uuid.uuid
    #     uuid = "".join([f'{i:x}' for i in uuid_list])
    
    for cmd in MachO.MachO(file_path).headers[0].commands:
        if cmd[0].cmd == mach_o.LC_UUID:
            uudi_bytes = cmd[1].uuid
            uuid = "".join([f'{i:02x}' for i in uudi_bytes])
            break
    return uuid


class ANSI:
    END = "\033[0m"
    BOLD = "\033[1m"
    RED = "\033[91m"
    BLACK = "\033[30m"
    BLUE = "\033[94m"
    BG_YELLOW = "\033[43m"
    BG_BLUE = "\033[44m"


def prt(msg, colors=ANSI.END):
    print(colors + f"{msg}" + ANSI.END)
    
def set_verbose(flag: bool):
    global VERBOSE
    VERBOSE = flag
    
def verbose() -> bool:
    return VERBOSE

def curr_interpreter() -> str:
    return sys.executable


if __name__ == "__main__":
    prt(DEFAULT_CONFIG, ANSI.BG_BLUE)
    set_record_parameter()
    prt(DEFAULT_CONFIG, ANSI.RED)
