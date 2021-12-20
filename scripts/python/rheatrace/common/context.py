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

import os


class Context:

    def __init__(self):
        pass

    @classmethod
    def instance(cls):
        if not hasattr(Context, "_instance"):
            Context._instance = Context()
        return Context._instance

    def set_options(self, options, categories, dict):
        self.options = options
        self.categories = categories
        self.environment_params = dict
        self.serial_number = options.device_serial_number
        self.output_file = options.output_file
        self.build_dir = os.path.dirname(os.path.abspath(options.output_file)) + os.sep + ".build"
        self.app_name = options.app_name
        self.trace_time = options.trace_time
        self.trace_buf_size = options.trace_buf_size
        self.show_version = options.version
        self.kfuncs = options.kfuncs
        self.from_file = options.from_file
        self.systrace_categories = options.systrace_categories
        self.restart = options.restart
        self.advanced_systrace_time = options.advanced_systrace_time
        self.list_categories = options.list_categories
        self.debug = options.debug

    def set_params(self, key, value):
        if self.environment_params is not None:
            self.environment_params[key] = value
        else:
            print "warning: dict is null!"

    def get_build_file_path(self, file_name):
        return self.build_dir + os.sep + file_name

    def get_params(self, key):
        if self.environment_params is not None:
            if self.environment_params.__contains__(key):
                return self.environment_params[key]
        else:
            print "warning: dict is null!"
        return False
