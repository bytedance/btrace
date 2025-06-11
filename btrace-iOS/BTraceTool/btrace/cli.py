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

import argparse
import atexit
import os
import signal
import sys
import time

from pick import pick

from btrace import app
from btrace import device
from btrace import util
from btrace.model import Statistics
from btrace.parse import parse
from btrace.export import export
from btrace.statistics import report

stat = Statistics()

def cmd_init(args):
    pass


def cmd_stop(args) -> str:
    print("stop recording.")
    
    app.check_connection()

    success = True
    trace_file = ""
    output_path = args.output
    trace_data = app.stop_record()
    
    if not output_path:
        home_dir = os.path.expanduser("~")
        output_path = os.path.join(home_dir, "Desktop")
        
        if not os.path.exists(output_path):
            output_path = os.getcwd()

    if not trace_data:
        success = False
        raise RuntimeError("Error, empty trace data!")
    else:
        curr_time = int(time.time())
        output_path = os.path.join(output_path, "btrace", args.bundle_id)
        os.makedirs(output_path, exist_ok=True)
        file_name = f"{stat.device_type}_{stat.os_version}_{stat.build_version}_{curr_time}.sqlite"
        trace_file = os.path.join(output_path, file_name)

        print(f"trace file: {trace_file}")

        with open(trace_file, "wb") as fp:
            fp.write(trace_data)

    stat.success = success
    report(stat)
    
    return trace_file


def cmd_record(args):

    pid_list = []
    atexit.register(util.clean_subprocess, pid_list)

    device_id = args.device_id
    bundle_id = args.bundle_id
    time_limit = args.time_limit or 3600
    hitch = args.hitch or 50
    hitch = max(hitch, 33)
    main_thread_only = args.main_thread_only
    launch = args.launch
    next_launch = args.next_launch
    verbose = args.verbose
    
    util.set_verbose(verbose)

    device_info = {}

    device_list = device.get_device_list()

    if not device_list or len(device_list) == 0:
        print("Error: could not find device!")
        return
    elif len(device_list) == 1:
        device_info = device_list[0]
        connected_device_id = device_info["Identifier"]
        
        if device_id:
            if connected_device_id != device_id:
                print("Error: connected device's id does not match the given device id!")
                return
        else:
            device_id = connected_device_id
    else:
        if device_id:
            for info in device_list:
                if info["Identifier"] == device_id:
                    device_info = info
                    break
            
            if not device_info:
                print("Error: could not find matched device!")
        else:
            title = "Please choose device: "
            options = [
                {
                    "name": x["DeviceName"] + " " + x["ProductVersion"],
                    "id": x["Identifier"],
                }
                for x in device_list
            ]
            option, index = pick(options, title)
            device_info = device_list[index]
            device_id = option["id"]

    stat.device_id = device_id
    stat.device_type = device_info["ProductType"]
    stat.os_version = device_info["ProductVersion"]
    stat.build_version = device_info["BuildVersion"]

    app_info = {}
    bundle_id_list = []

    if bundle_id:
        bundle_id_list.append(bundle_id)

    app_list = app.get_supported_app_list(device_id, bundle_id_list)

    if not app_list or len(app_list) == 0:
        print("Error: could not find app!")
        return
    elif len(app_list) == 1:
        app_info = app_list[0]
        bundle_id = app_info["CFBundleIdentifier"]
    else:
        title = "Please choose app: "
        app_dict = {x["CFBundleIdentifier"]: x for x in app_list}
        options = [
            {"name": x["CFBundleDisplayName"], "id": x["CFBundleIdentifier"]}
            for x in app_list
        ]
        option, index = pick(options, title)
        bundle_id = option["id"]
        app_info = app_dict[bundle_id]

    stat.bundle_id = bundle_id
    args.bundle_id = bundle_id

    pid = device.connect_if_need(device_id)
    pid_list.append(pid)

    util.set_record_parameter(main_thread_only, hitch)

    if launch:
        app.launch(device_id, bundle_id)
    elif next_launch:
        app.check_connection()
        app.upload_config()
    else:
        app.check_connection()
        # stop before start
        app.stop_record()
        app.start_record()

    print(f"start recording, bundle id: {bundle_id}")

    should_exit = False
    count = 0

    def signal_stop(signum, frame):
        nonlocal should_exit
        should_exit = True

    signal.signal(signal.SIGINT, signal_stop)

    print(f"Press cltr+c to stop")

    interval = 0.5
    record_time = 0
    start_time = time.time()

    while (not should_exit) and record_time < time_limit:
        count += 1
        time.sleep(interval)
        curr_time = time.time()
        record_time = curr_time - start_time

        if count % 2 == 0:
            print("\r" + f"record {record_time:.1f}s", end="")

    print("\n")
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    trace_file = cmd_stop(args)

    if args.dsym_path:
        sys_symbol = args.sys_symbol
        parse(trace_file, args.dsym_path, True, sys_symbol)


def cmd_parse(args):
    file_path = args.file_path
    dsym_path = args.dsym_path
    sys_symbol = args.sys_symbol
    force = args.force
    verbose = args.verbose
    
    util.set_verbose(verbose)
    
    parse(file_path, dsym_path, force, sys_symbol)
    
    
def cmd_export(args):
    file_path = args.file_path
    dsym_path = args.dsym_path
    output_path = args.output
    export_type = args.export_type or "sqlite"
    sys_symbol = args.sys_symbol
    force = args.force
    verbose = args.verbose
    
    util.set_verbose(verbose)
    
    export(file_path, dsym_path, output_path, export_type, force, sys_symbol)


def cmd(args):

    subparser: str = args.subparser

    if subparser == "record":
        cmd_record(args)
    elif subparser == "parse":
        cmd_parse(args)
    elif subparser == "init":
        cmd_init(args)
    elif subparser == "export":
        cmd_export(args)


def main():
    parser = argparse.ArgumentParser(description="BTrace command line tool.")

    subparser = parser.add_subparsers(dest="subparser", help="sub-command help")

    # init
    init_parser = subparser.add_parser("init", help="init")

    # record
    record_parser = subparser.add_parser("record", help="record trace")
    record_parser.add_argument("-i", "--device_id", dest="device_id", help="device id")
    record_parser.add_argument("-b", "--bundle_id", dest="bundle_id", help="bundle id")
    record_parser.add_argument(
        "-o", "--output", dest="output", help="trace file output path"
    )
    record_parser.add_argument(
        "-t", "--time_limit", dest="time_limit", type=int, help="record time limit"
    )
    record_parser.add_argument(
        "-H", "--hitch", dest="hitch", type=int, help="hitch thres"
    )
    record_parser.add_argument("-d", "--dsym_path", dest="dsym_path", help="dsym dir")
    record_parser.add_argument(
        "-m",
        "--main_thread_only",
        dest="main_thread_only",
        action="store_true",
        help="main thread only",
    )
    record_parser.add_argument(
        "-l", "--launch", dest="launch", action="store_true", help="launch app"
    )
    record_parser.add_argument(
        "-n", "--next_launch", dest="next_launch", action="store_true", help="record trace after launching the app next time"
    )
    record_parser.add_argument(
        "-s", "--sys_symbol", dest="sys_symbol", action="store_true", help="parse sys symbol"
    )
    record_parser.add_argument(
        "-v", "--verbose", dest="verbose", action="store_true", help="show verbose info"
    )

    # parse
    parser_lib = subparser.add_parser("parse", help="parse trace data")
    parser_lib.add_argument("file_path", help="file path")
    parser_lib.add_argument("-d", "--dsym_path", dest="dsym_path", help="dsym dir path")
    parser_lib.add_argument(
        "-f", "--force", dest="force", action="store_true", help="force parse"
    )
    parser_lib.add_argument(
        "-s", "--sys_symbol", dest="sys_symbol", action="store_true", help="parse sys symbol"
    )
    parser_lib.add_argument(
        "-v", "--verbose", dest="verbose", action="store_true", help="show verbose info"
    )
    
    # export
    export_lib = subparser.add_parser("export", help="export trace data")
    export_lib.add_argument("file_path", help="file path")
    export_lib.add_argument("-d", "--dsym_path", dest="dsym_path", help="dsym dir path")
    export_lib.add_argument("-o", "--output", dest="output", help="export output path")
    export_lib.add_argument("-t", "--type", dest="export_type", help="output data type")
    export_lib.add_argument(
        "-f", "--force", dest="force", action="store_true", help="force export"
    )
    export_lib.add_argument(
        "-s", "--sys_symbol", dest="sys_symbol", action="store_true", help="export sys symbol"
    )
    export_lib.add_argument(
        "-v", "--verbose", dest="verbose", action="store_true", help="show verbose info"
    )

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)

    args = parser.parse_args()

    cmd(args)


if __name__ == "__main__":
    # sys.argv = ["btrace", "record", "--launch"]
    main()
