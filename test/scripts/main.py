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
# pytest-xdist, pytest-ordering, pytest-rerunfailures, setuptools, Jinja2, requests
# 准备文件：package.zip
# 执行方式：python main.py

import subprocess
import pytest
import os
import time
import prepare
from testModule.utils import GP, check_library_installation, pytest_run, load_gp


def get_version_file():
    version_file_path = os.path.join(os.getcwd(), "resource", "version")
    if os.path.exists(version_file_path):
        with open(version_file_path, "r", encoding="UTF-8") as version_file:
            version = version_file.read().strip()
            return version
    else:
        return "unknown"


def is_gen_conf():
    config_file_path = os.path.join(os.getcwd(), ".hdctester.conf")
    return os.path.exists(config_file_path)


def main():
    if check_library_installation("pytest"):
        subprocess.check_call(["pip", "install", "-r", "requirements.txt"])
        if check_library_installation("pytest"):
            return
    start_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(time.time()))
    if get_version_file() == "unknown":
        prepare.prepare()
    if not is_gen_conf():
        GP.init()
    pytest_run()


if __name__ == '__main__':
    main()