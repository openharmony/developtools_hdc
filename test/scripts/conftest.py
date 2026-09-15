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
import subprocess

import pytest


@pytest.fixture(autouse=True, scope="session")
def update_device_sn_fixture():
    update_device_sn()


def update_device_sn():
    """
    全部用例执行前重新枚举设备列表，按当前 SN 刷 GP.device_name 与 GP.hdc_head。
    防止测试开始时设备 SN 与初始化时不一致导致后续 hdc -t 失败。
    """
    try:
        from testModule.utils import GP
        out = subprocess.check_output([GP.hdc_exe, "list targets"], stderr=subprocess.STDOUT).decode().strip()
        targets = [t for t in out.splitlines() if t and t != "[empty]" and "failed" not in t]
        if not targets:
            return
        new_sn = targets[0].strip()
        if new_sn and new_sn != GP.device_name:
            old_sn = GP.device_name
            GP.device_name = new_sn
            GP.hdc_head = f"{GP.hdc_exe} -t {GP.device_name}"
            logging.info("update_device_sn: %s -> %s, hdc_head=%s", old_sn, new_sn, GP.hdc_head)
    except Exception as e:
        logging.warning("update_device_sn failed: %s", e)


def pytest_configure(config):
    file_time = time.strftime('%Y%m%d_%H%M%S', time.localtime(time.time()))
    file_dir = "reports"
    if not os.path.exists(file_dir):
        os.mkdir(file_dir)
    log_file_name = f"hdc_test_{file_time}.log"
    logging.basicConfig(
        level=logging.DEBUG,
        filename=os.path.join(file_dir, log_file_name),
        format="[%(asctime)s %(name)s %(funcName)s %(lineno)d %(levelname)s][%(process)d][%(thread)d][%(threadName)s]"
               "%(message)s"
    )


if __name__ == '__main__':
    pass