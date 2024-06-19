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
from dev_hdc_test import make_multiprocess_file, rmdir
from dev_hdc_test import check_app_install_multi, check_app_uninstall_multi
from dev_hdc_test import check_rom, check_shell


def test_list_targets():
    assert check_hdc_targets()
    assert check_hdc_cmd("shell rm -rf data/local/tmp/it_*")


@pytest.mark.repeat(5)
def test_empty_file():
    assert check_hdc_cmd(f"file send {get_local_path('empty')} {get_remote_path('it_empty')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_empty')} {get_local_path('empty_recv')}")


@pytest.mark.repeat(5)
def test_small_file():
    assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('it_small')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_small')} {get_local_path('small_recv')}")


@pytest.mark.repeat(1)
def test_node_file():
    assert check_hdc_cmd(f"file recv {get_remote_path('../../../sys/power/state')} {get_local_path('state')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('../../../sys/firmware/fdt')} {get_local_path('fdt')}")


@pytest.mark.repeat(1)
def test_medium_file():
    assert check_hdc_cmd(f"file send {get_local_path('medium')} {get_remote_path('it_medium')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_medium')} {get_local_path('medium_recv')}")


@pytest.mark.repeat(1)
def test_large_file():
    assert check_hdc_cmd(f"file send {get_local_path('large')} {get_remote_path('it_large')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_large')} {get_local_path('large_recv')}")


@pytest.mark.repeat(1)
def test_file_error():
    assert check_hdc_cmd("shell mount -o rw,remount /")
    assert check_shell(
        f"file send {get_local_path('small')} system/bin/hdcd",
        "busy"
        )
    assert check_shell(
        f"file recv",
        "[Fail]There is no local and remote path"
    )
    assert check_shell(
        f"file send",
        "[Fail]There is no local and remote path"
    )
    assert check_shell(
        f"file send {get_local_path('large')} {get_remote_path('../../../')}",
        "space left on device"
    )
    assert check_hdc_cmd(f"shell rm -rf {get_remote_path('../../../large')}")
    assert check_hdc_cmd(f"shell param set persist.hdc.control.file false")
    assert check_shell(
        f"file send {get_local_path('small')} {get_remote_path('it_small_false')}",
        "debugging is not allowed"
    )
    assert check_hdc_cmd(f"shell param set persist.hdc.control.file true")
    assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('it_small_true')}")


@pytest.mark.repeat(1)
def test_recv_dir():
    assert make_multiprocess_file(get_local_path('problem_dir'), get_remote_path(''), 'send', 1, "dir")
    assert check_hdc_cmd(f"shell mv {get_remote_path('problem_dir')} {get_remote_path('it_problem_dir')}")
    assert make_multiprocess_file(get_local_path(''), get_remote_path('it_problem_dir'), 'recv', 1, "dir")
    if os.path.exists(get_local_path('it_problem_dir')):
        rmdir(get_local_path('it_problem_dir'))


@pytest.mark.repeat(5)
def test_hap_install():
    assert check_hdc_cmd(f"install -r {get_local_path('entry-default-signed-debug.hap')}",
                            bundle="com.hmos.diagnosis")


@pytest.mark.repeat(5)
def test_app_cmd():
    package_hap = "entry-default-signed-debug.hap"
    app_name_default = "com.hmos.diagnosis"

    # default
    assert check_app_install(package_hap, app_name_default)
    assert check_app_uninstall(app_name_default)

    # -r
    assert check_app_install(package_hap, app_name_default, "-r")
    assert check_app_uninstall(app_name_default)

    # -k
    assert check_app_install(package_hap, app_name_default, "-r")
    assert check_app_uninstall(app_name_default, "-k")

    # -s
    package_hap = "analyticshsp-default-signed.hsp"
    app_name_default = "com.huawei.hms.hsp.analyticshsp"

    assert check_app_install(package_hap, app_name_default, "-s")
    assert check_app_uninstall(app_name_default, "-s")

    # default multi hap
    tables = {
        "entry-default-signed-debug.hap" : "com.hmos.diagnosis",
        "ActsAudioRecorderJsTest.hap" : "ohos.acts.multimedia.audio.audiorecorder"
    }
    assert check_app_install_multi(tables)
    assert check_app_uninstall_multi(tables)

    # default multi hap -r -k
    tables = {
        "entry-default-signed-debug.hap" : "com.hmos.diagnosis",
        "ActsAudioRecorderJsTest.hap" : "ohos.acts.multimedia.audio.audiorecorder"
    }
    assert check_app_install_multi(tables, "-r")
    assert check_app_uninstall_multi(tables, "-k")

    # default multi hsp -s
    tables = {
        "libA_v10001.hsp" : "com.example.liba",
        "libB_v10001.hsp" : "com.example.libb",
    }
    assert check_app_install_multi(tables, "-s")
    assert check_app_uninstall_multi(tables, "-s")


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
    rport_list = []
    start_port = 10000
    end_port = 10020
    for i in range(start_port, end_port):
        fport = f"tcp:{i+100} tcp:{i+200}"
        rport = f"tcp:{i+300} tcp:{i+400}"
        localabs = f"tcp:{i+500} localabstract:{f'helloworld.com.app.{i+600}'}"
        fport_list.append(fport)
        rport_list.append(rport)
        fport_list.append(localabs)
    
    for fport in fport_list:
        assert check_hdc_cmd(f"fport {fport}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport {fport}", "TCP Port listen failed at")
        assert check_hdc_cmd("fport ls", fport)

    for fport in fport_list:
        assert check_hdc_cmd(f"fport rm {fport}", "success")
        assert not check_hdc_cmd("fport ls", fport)

    for rport in rport_list:
        assert check_hdc_cmd(f"rport {rport}", "Forwardport result:OK")
        assert check_hdc_cmd(f"rport {rport}", "TCP Port listen failed at")
        assert check_hdc_cmd("rport ls", rport) or check_hdc_cmd("fport ls", rport)

    for rport in rport_list:
        assert check_hdc_cmd(f"rport rm {rport}", "success")
        assert not check_hdc_cmd("rport ls", fport) and not check_hdc_cmd("fport ls", fport)

    task_str1 = "tcp:33333 tcp:33333"
    assert check_hdc_cmd(f"fport {task_str1}", "Forwardport result:OK")
    assert check_hdc_cmd(f"fport rm {task_str1}", "success")
    assert check_hdc_cmd(f"fport {task_str1}", "Forwardport result:OK")
    assert check_hdc_cmd(f"fport rm {task_str1}", "success")

    task_str2 = "tcp:44444 tcp:44444"
    assert check_hdc_cmd(f"rport {task_str2}", "Forwardport result:OK")
    assert check_hdc_cmd(f"rport rm {task_str2}", "success")
    assert check_hdc_cmd(f"rport {task_str2}", "Forwardport result:OK")
    assert check_hdc_cmd(f"rport rm {task_str2}", "success")    


def test_shell_cmd_timecost():
    assert check_cmd_time(
        cmd="shell \"ps -ef | grep hdcd\"",
        pattern="hdcd",
        duration=None,
        times=10)


def test_hdcd_rom():
    baseline = 2200 # 2200KB
    assert check_rom(baseline)


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
