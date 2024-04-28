#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2024 Huawei Device Co., Ltd.
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
# 运行环境: python 3.10+, pytest, pytest-repeat, pytest-testreport
# 准备文件：package.zip
# pip install pytest pytest-testreport pytest-repeat
# python hdc_normal_test.py


import argparse
import time
import os

import pytest

from dev_hdc_test import GP
from dev_hdc_test import check_library_installation, check_hdc_version, check_cmd_time
from dev_hdc_test import check_hdc_cmd, check_hdc_targets, get_local_path, get_remote_path
from dev_hdc_test import check_app_install, check_app_uninstall, prepare_source, pytest_run


def test_list_targets():
    assert check_hdc_targets()


@pytest.mark.repeat(5)
def test_empty_file():
    assert check_hdc_cmd(f"file send {get_local_path('empty')} {get_remote_path('it_empty')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_empty')} {get_local_path('empty_recv')}")


@pytest.mark.repeat(5)
def test_small_file():
    assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('it_small')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_small')} {get_local_path('small_recv')}")


@pytest.mark.repeat(1)
def test_medium_file():
    assert check_hdc_cmd(f"file send {get_local_path('medium')} {get_remote_path('it_medium')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_medium')} {get_local_path('medium_recv')}")


@pytest.mark.repeat(1)
def test_large_file():
    assert check_hdc_cmd(f"file send {get_local_path('large')} {get_remote_path('it_large')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_large')} {get_local_path('large_recv')}")


@pytest.mark.repeat(5)
def test_hap_install():
    assert check_hdc_cmd(f"install -r {get_local_path('entry-default-signed-debug.hap')}",
                            bundle="com.hmos.diagnosis")


@pytest.mark.repeat(5)
def test_app_cmd():
    package_hap = "entry-default-signed-debug.hap"
    app_name_default = "com.hmos.diagnosis"

    assert check_app_install(package_hap, app_name_default)
    assert check_app_uninstall(app_name_default)

    assert check_app_install(package_hap, app_name_default, "-r")
    assert check_app_uninstall(app_name_default)


def test_server_kill():
    assert check_hdc_cmd("kill", "Kill server finish")
    assert check_hdc_cmd("start server", "")


def test_target_cmd():
    assert check_hdc_targets()    
    time.sleep(3)
    check_hdc_cmd("target boot")
    time.sleep(60) # reboot needs at least 60 seconds
    assert (check_hdc_cmd("target mount", "Mount finish") or
            check_hdc_cmd("target mount", "[Fail]Operate need running as root") or
            check_hdc_cmd("target mount", "Remount successful.")
            )


def test_version_cmd():
    version = "Ver: 2.0.0a"
    assert check_hdc_version("-v", version)
    assert check_hdc_version("version", version)
    assert check_hdc_version("checkserver", version)


def test_fport_cmd():
    fport_list = []
    start_port = 10000
    end_port = 10020
    for i in range(start_port, end_port):
        fport = f"tcp:{i+100} tcp:{i+200}"
        rport = f"tcp:{i+300} tcp:{i+400}"
        localabs = f"tcp:{i+500} localabstract:{f'helloworld.com.app.{i+600}'}"
        fport_list.append(fport)
        fport_list.append(rport)
        fport_list.append(localabs)
    
    for fport in fport_list:
        assert check_hdc_cmd(f"fport {fport}", "Forwardport result:OK")
        assert check_hdc_cmd("fport ls", fport)

    for fport in fport_list:
        assert check_hdc_cmd(f"fport rm {fport}", "success")
        assert not check_hdc_cmd("fport ls", fport)

def test_shell_cmd_timecost():
    assert check_cmd_time(
        cmd="shell \"ps -ef | grep hdcd\"",
        pattern="hdcd",
        duration=150,
        times=10)

def setup_class():
    print("setting up env ...")
    check_hdc_cmd("shell rm -rf data/local/tmp/it_*")
    GP.load()


def teardown_class():
    pass


def run_main():
    if check_library_installation("pytest"):
        exit(1)

    if check_library_installation("pytest-testreport"):
        exit(1)
    
    if check_library_installation("pytest-repeat"):
        exit(1)

    GP.init()

    if not os.path.exists(GP.local_path):
        prepare_source()

    choice_default = ""
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=1,
                        help='test times')
    parser.add_argument('--verbose', '-v', default=__file__,
                        help='filename')
    parser.add_argument('--desc', '-d', default='Test for function.',
                        help='Add description on report')
    args = parser.parse_args()
    
    pytest_run(args)


if __name__ == "__main__":
    run_main()