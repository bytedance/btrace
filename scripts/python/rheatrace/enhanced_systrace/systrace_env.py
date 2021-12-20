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


def get_executable_systrace():
    systrace_file_name = "systrace.py"
    paths = os.environ["PATH"].split(os.pathsep)
    for path in paths:
        systrace_path = os.path.join(path, systrace_file_name)
        if os.path.isfile(systrace_path):
            return systrace_path
    return None
