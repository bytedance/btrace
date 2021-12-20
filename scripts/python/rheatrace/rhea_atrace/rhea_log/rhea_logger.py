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
import logging
import time

import rhea_config

logger = logging.getLogger('rhea_trace')
logging_format = logging.Formatter(
    '%(asctime)s - %(pathname)s - %(funcName)s - %(lineno)s - %(levelname)s: %(message)s')


def set_logger(log_level, output_dir):
    os.popen(
        "find {} -name '*.log' -type f -print -exec rm -rf {{}} \;".format(output_dir))
    logging.basicConfig(format='%(asctime)s - %(name)s - %(funcName)s - %(levelname)s: - %(message)s')
    if rhea_config.LOG_IS_SAVE:
        fh = logging.FileHandler(r'{0}/_log_{1}.log'.
                                 format(output_dir,
                                        time.strftime("%Y-%m-%d-%H:%M:%S",
                                                      time.localtime()),
                                        encoding='utf-8'))
        fh.setFormatter(logging_format)
        logger.addHandler(fh)
    logger.setLevel(rhea_config.LOG_LEVEL)
    if log_level is not None:
        logger.setLevel(log_level)
    if not rhea_config.LOG_IS_OPEN:
        logger.disabled = True


def get_logger():
    return logger


rhea_logger = get_logger()
