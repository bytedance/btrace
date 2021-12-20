#!/usr/bin/env python

# Copyright (C) 2021 ByteDance Inc
#
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
#

"""
Usage: rhea.py [options] [category1 [category2 ...]]

Example: rheatrace.py -b 32768 -t 15 -o trace.html -a

Options:
  -h, --help            show this help message and exit
  -a APP_NAME, --app=APP_NAME
                        enable application-level tracing for comma-separated
                        list of app cmdlines
  -e DEVICE_SERIAL_NUMBER, --serial=DEVICE_SERIAL_NUMBER
                        adb device serial number
  -b N, --buf-size=N    use a trace buffer size of N KB
  -t N, --time=N        trace for N seconds
  -o FILE               write trace output to FILE
  -k KFUNCS, --ktrace=KFUNCS
                        specify a comma-separated list of kernel functions to
                        trace
  --from-file File      read the trace from a file (compressed) rather than'
                        'running a live trace
  --categories=SYSTRACE_CATEGORIES
                        Select categories with a comma-delimited list, e.g.
                        cat1,cat2,cat3
  -l, --list-categories
                        list the available categories and exit

  RheaTrace options:
    -v, --version       rhea script version and exit.
    -r, restart         if app is running, it will be killed and restart.
"""

import os
import sys
import logging
import shutil
import time
import multiprocessing
import optparse
import rhea_config
from common.context import Context
from common import env_checker
from enhanced_systrace import systrace_capturer
from rhea_atrace import atrace_capturer
from common import cmd_executer
from rhea_atrace.rhea_log.rhea_logger import rhea_logger, set_logger
from trace_processor import TraceProcessor

logger = rhea_logger


def add_sys_options(parser):
    parser.add_option('-a', '--app', dest='app_name', default=None, type='string', action='store',
                      help='enable application-level tracing for comma-separated list of app cmdlines')
    parser.add_option('-e', '--serial', dest='device_serial_number', type='string', help='adb device serial number')
    parser.add_option('-b', '--buf-size', dest='trace_buf_size',
                      type='int', default='32768', help='use a trace buffer size of N KB', metavar='N')
    parser.add_option('-t', '--time', dest='trace_time', type='int',
                      help='trace for N seconds', metavar='N')
    parser.add_option('-o', dest='output_file', help='write trace output to FILE',
                      default=None, metavar='FILE')
    parser.add_option('-k', '--ktrace', dest='kfuncs', action='store',
                      help='specify a comma-separated list of kernel functions '
                           'to trace')
    parser.add_option('--from-file', dest='from_file', action='store',
                      help='read the trace from a file (compressed) rather than'
                           'running a live trace')
    parser.add_option('--categories', dest='systrace_categories',
                      help='Select categories with a comma-delimited '
                           'list, e.g. cat1,cat2,cat3')
    parser.add_option('-l', '--list-categories', dest='list_categories',
                      default=False, action='store_true',
                      help='list the available categories and exit')
    return parser


def add_extra_options(parser):
    options = optparse.OptionGroup(parser, 'RheaTrace options')
    options.add_option('-v', '--version', dest='version', action="store_true", default=False,
                       help="rhea script version and exit")
    parser.add_option('--advanced-sys-time', dest='advanced_systrace_time', type='int',
                      default='2', help='advanced systrace time for N seconds', metavar='N')
    parser.add_option('-r', '--restart', dest='restart',
                      default=False, action='store_true',
                      help='if app is running, it will be killed and restart.')
    parser.add_option('--debug', dest='debug',
                      default=False, action='store_true',
                      help='Set the log switch to debug level.')
    return options


def parse_options(argv):
    """Parses and checks the command-line options."""
    usage = 'Usage: %prog [options] [category1 [category2 ...]]'
    desc = 'Example: %prog -b 32768 -t 15 gfx input view sched freq'
    parser = optparse.OptionParser(usage=usage, description=desc,
                                   conflict_handler="resolve")
    parser = add_sys_options(parser)
    option_group = add_extra_options(parser)
    if option_group:
        parser.add_option_group(option_group)
    (options, categories) = parser.parse_args(argv[1:])

    """arg check"""
    if options.output_file is None:
        options.output_file = rhea_config.DEFAULT_OUTPUT_FILE
    context = Context.instance()
    context.set_options(options, categories, multiprocessing.Manager().dict())
    return context


def _initialize_devices(context):
    from devil.android.sdk import adb_wrapper
    from systrace import run_systrace

    run_systrace.initialize_devil()
    devices = [device.GetDeviceSerial() for device in adb_wrapper.AdbWrapper.Devices()]
    if context.serial_number is None:
        if len(devices) == 0:
            logger.error('no adb devices connected.')
            return False
        elif len(devices) == 1:
            context.serial_number = devices[0]
            return True
        elif len(devices) >= 2:
            logger.error('multiple devices connected, serial number required')
            return False

    elif context.serial_number not in devices:
        logger.error('Device with the serial number "%s" is not connected.'
                     % context.serial_number)
        return False
    return True


def remove_all_stale_pyc_files(base_dir):
    """Scan directories for old .pyc files without a .py file and delete them."""
    for dirname, _, filenames in os.walk(base_dir):
        if '.git' in dirname:
            continue
        for filename in filenames:
            root, ext = os.path.splitext(filename)
            if ext != '.pyc':
                continue

            pyc_path = os.path.join(dirname, filename)
            py_path = os.path.join(dirname, root + '.py')

            try:
                if not os.path.exists(py_path):
                    os.remove(pyc_path)
            except OSError:
                # Wrap OS calls in try/except in case another process touched this file.
                pass

        try:
            os.removedirs(dirname)
        except OSError:
            # Wrap OS calls in try/except in case another process touched this dir.
            pass


def init_build_dir(context):
    if os.path.exists(context.build_dir) and os.path.isdir(context.build_dir):
        shutil.rmtree(context.build_dir)
    os.makedirs(context.build_dir)


def show_version():
    """
    show current rheatrace script version
    """
    print "Current version is %s.\n" % rhea_config.VERSION_CODE
    sys.exit(0)


def main_impl(argv):
    """check environment variables, if not satisfied, will exit"""
    is_python_2_7 = env_checker.check_python()
    if is_python_2_7 is False:
        sys.exit(1)
    remove_all_stale_pyc_files(os.path.dirname(__file__))
    """parse input options."""
    context = parse_options(argv)
    """clean and mkdirs"""
    init_build_dir(context)
    set_logger(None, context.build_dir)
    if context.debug:
        set_logger(logging.DEBUG, context.build_dir)
    if context.show_version:
        show_version()
        sys.exit(1)
    elif context.list_categories:
        (out, return_code) = systrace_capturer.show_list_categories(context.serial_number)
        logger.info("\n" + out)
        sys.exit(1)
    elif context.trace_time < 0:
        logger.error('trace time must be specified or a non-negative number')
        sys.exit(1)
    elif context.trace_buf_size < 0:
        logger.error('trace buffer size must be a positive number')
        sys.exit(1)
    elif context.advanced_systrace_time < 0:
        logger.error('advanced systrace time must be a positive number')
        sys.exit(1)
    elif context.from_file:
        (out, return_code) = systrace_capturer.from_file(context.from_file)
        logger.info("\n" + out)
        sys.exit(1)
    else:
        if context.app_name is None or "":
            logger.error("app name must be specified, using '-a' to set app name.")
            sys.exit(1)
        env_ok = env_checker.check_env()
        if not env_ok:
            sys.exit(1)
        if not _initialize_devices(context):
            sys.exit(1)

        """delete rheatrace.stop file"""
        if __delete_rheatrace_stop_file(context) is False:
            logger.debug("failed to delete rhea-atrace.stop file, maybe it's not exist.")

        """check whether app is installed or not."""
        result = __check_install(context.app_name, context.serial_number)
        if not result:
            logger.warning("app '%s' is not installed, please check your inputs.", context.app_name)
            sys.exit(1)

        """start to capture systrace"""
        _systrace_capturer = multiprocessing.Process(target=systrace_capturer.capture, args={context})
        _systrace_capturer.start()
        time.sleep(context.advanced_systrace_time)

        """launch app and capture atrace"""
        _atrace_capturer = multiprocessing.Process(target=atrace_capturer.capture, args={context})
        _atrace_capturer.start()

        _systrace_capturer.join()
        _atrace_capturer.join()

        logger.debug("waiting for writing rhea-atrace.gz file.")
        time_loop = 0
        while time_loop < 20:
            if __is_rheatrace_stop_file_exist(context):
                logger.debug("finish to write rhea-atrace.gz file.")
                break
            else:
                time_loop = time_loop + 2
                time.sleep(2)
        if not __is_rheatrace_stop_file_exist(context):
            logger.error("failed to write rhea-atrace.gz file completely.")
        trace_processor = TraceProcessor(context)
        trace_processor.processor()


def __check_install(app_name, serial_number):
    if app_name is None:
        return False
    cmd = ["shell", "pm", "path", app_name]
    (output, return_code) = cmd_executer.exec_commands(cmd_executer.get_complete_abd_cmd(cmd, serial_number))
    if len(output) > 0:
        return True
    else:
        return False


def __is_rheatrace_stop_file_exist(context):
    commands = [
        "ls " + rhea_config.ATRACE_APP_GZ_FILE_LOCATION.replace("%s", context.app_name) + "rheatrace.stop",
        "exit"
    ]
    (out, return_code) = cmd_executer.exec_adb_shell_with_append_commands(commands, context.serial_number)
    if return_code is 0:
        return True
    else:
        return False


def __delete_rheatrace_stop_file(context):
    commands = [
        "cd " + rhea_config.ATRACE_APP_GZ_FILE_LOCATION.replace("%s", context.app_name),
        "rm rheatrace.stop",
        "exit"
    ]
    (out, return_code) = cmd_executer.exec_adb_shell_with_append_commands(commands, context.serial_number)
    if return_code is 0:
        return True
    else:
        return False


if __name__ == "__main__":
    main_impl(sys.argv)
