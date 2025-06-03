#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2025 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 运行环境: python 3.10+, pytest, pytest-repeat, pytest-testreport allure-pytest

import os
import logging
import time


def pytest_configure(config):
    file_time = time.strftime('%Y%m%d_%H%M%S', time.localtime(time.time()))
    log_file_name = f"hdc_test_{file_time}.log"
    logging.basicConfig(
        level=logging.DEBUG,
        filename=os.path.join("reports", log_file_name),
        format="[%(asctime)s %(name)s %(funcName)s %(lineno)d %(levelname)s][%(process)d][%(thread)d][%(threadName)s]"
               "%(message)s"
    )


if __name__ == '__main__':
    pass