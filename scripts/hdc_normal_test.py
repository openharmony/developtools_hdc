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
import multiprocessing

import pytest

from dev_hdc_test import GP
from dev_hdc_test import check_library_installation, check_hdc_version, check_cmd_time
from dev_hdc_test import check_hdc_cmd, check_hdc_targets, get_local_path, get_remote_path, run_command_with_timeout
from dev_hdc_test import check_app_install, check_app_uninstall, prepare_source, pytest_run, update_source, check_rate
from dev_hdc_test import make_multiprocess_file, rmdir, get_shell_result
from dev_hdc_test import check_app_install_multi, check_app_uninstall_multi
from dev_hdc_test import check_rom, check_shell, check_shell_any_device, check_cmd_block


def test_hdc_server_foreground():
    port = os.getenv('OHOS_HDC_SERVER_PORT')
    if port is None:
        port = 8710
    assert check_hdc_cmd("kill", "Kill server finish")
    assert check_cmd_block(f"{GP.hdc_exe} -m", f"port: {port}", timeout=5)
    assert check_hdc_cmd("start")
    time.sleep(3) # sleep 3s to wait for the device to connect channel


def test_list_targets():
    assert check_hdc_targets()
    assert check_hdc_cmd("shell rm -rf data/local/tmp/it_*")
    assert check_hdc_cmd("shell mkdir data/local/tmp/it_send_dir")


def test_usb_disconnect():
    assert check_hdc_targets()
    cmd = 'shell "kill -9 `pidof hdcd`"'
    check_hdc_cmd(f"{cmd}", "[Fail][E001003] USB communication abnormal, please check the USB communication link.")
    time.sleep(2)
    assert check_hdc_targets()


def test_list_targets_multi_usb_device():
    devices_str = check_shell_any_device(f"{GP.hdc_exe} list targets", None, True)
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    devices_array = devices_str[1].split('\n')
    if devices_array:
        for device in devices_array:
            if len(device) > 8:
                assert check_shell_any_device(f"{GP.hdc_exe} -t {device} shell id", "u:r:")[0]


@pytest.mark.repeat(5)
def test_empty_file():
    assert check_hdc_cmd(f"file send {get_local_path('empty')} {get_remote_path('it_empty')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_empty')} {get_local_path('empty_recv')}")


@pytest.mark.repeat(5)
def test_empty_dir():
    assert check_shell(f"file send {get_local_path('empty_dir')} {get_remote_path('it_empty_dir')}",
        "the source folder is empty")
    assert check_hdc_cmd("shell mkdir data/local/tmp/it_empty_dir_recv")
    assert check_shell(f"file recv {get_remote_path('it_empty_dir_recv')} {get_local_path('empty_dir_recv')}",
        "the source folder is empty")


@pytest.mark.repeat(5)
def test_long_path():
    assert check_hdc_cmd(f"file send {get_local_path('deep_test_dir')} {get_remote_path('it_send_dir')}",
                         is_single_dir=False)
    assert check_hdc_cmd(f"file recv {get_remote_path('it_send_dir/deep_test_dir')} {get_local_path('recv_test_dir')}",
                         is_single_dir=False)


@pytest.mark.repeat(3)
def test_no_exist_path():
    test_resource_list = ['empty', 'medium', 'small', 'problem_dir']
    remote_unexist_path = f"{get_remote_path('it_no_exist/deep_test_dir/')}"
    for test_item in test_resource_list:
        check_hdc_cmd(f"shell rm -rf {get_remote_path('it_no_exist*')}")
        local_unexist_path = get_local_path(f'{(os.path.join("recv_no_exist", "deep_test_dir", f"recv_{test_item}"))}')
        if os.path.exists(get_local_path('recv_no_exist')):
            rmdir(get_local_path('recv_no_exist'))
        assert check_hdc_cmd(f"file send "
                             f"{get_local_path(f'{test_item}')} {remote_unexist_path}/it_{test_item}")
        assert check_hdc_cmd(f"file recv {remote_unexist_path}/it_{test_item} "
                             f"{local_unexist_path}")


@pytest.mark.repeat(3)
def test_no_exist_path_with_seperate():
    test_resource_list = ['empty', 'medium', 'small']
    remote_unexist_path = f"{get_remote_path('it_no_exist/deep_test_dir/')}"
    for test_item in test_resource_list:
        check_hdc_cmd(f"shell rm -rf {get_remote_path('it_no_exist*')}")
        local_unexist_path = get_local_path(f'{(os.path.join("recv_no_exist", "deep_test_dir"))}')
        if os.path.exists(get_local_path('recv_no_exist')):
            rmdir(get_local_path('recv_no_exist'))
        assert check_hdc_cmd(f"file send "
                             f"{get_local_path(f'{test_item}')} {remote_unexist_path}/")
        assert check_hdc_cmd(f"file recv {remote_unexist_path}/{test_item} "
                             f"{local_unexist_path}{os.sep}")


@pytest.mark.repeat(5)
def test_small_file():
    assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('it_small')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_small')} {get_local_path('small_recv')}")


@pytest.mark.repeat(5)
def test_small_file_compress():
    assert check_hdc_cmd(f"file send -z {get_local_path('small')} {get_remote_path('it_small_z')}")
    assert check_hdc_cmd(f"file recv -z {get_remote_path('it_small_z')} {get_local_path('small_z_recv')}") 


@pytest.mark.repeat(1)
def test_node_file():
    assert check_hdc_cmd(f"file recv {get_remote_path('../../../sys/power/state')} {get_local_path('state')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('../../../sys/firmware/fdt')} {get_local_path('fdt')}")


@pytest.mark.repeat(1)
def test_medium_file():
    assert check_hdc_cmd(f"file send {get_local_path('medium')} {get_remote_path('it_medium')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_medium')} {get_local_path('medium_recv')}")


@pytest.mark.repeat(1)
def test_medium_file_compress():
    assert check_hdc_cmd(f"file send -z {get_local_path('medium')} {get_remote_path('it_medium_z')}")
    assert check_hdc_cmd(f"file recv -z {get_remote_path('it_medium_z')} {get_local_path('medium_z_recv')}")


@pytest.mark.repeat(1)
def test_medium_file_compress2():
    assert check_hdc_cmd(f"file send -z {get_local_path('word_100M.txt')} {get_remote_path('word_100M_compress.txt')}")
    assert check_hdc_cmd(f"file recv -z {get_remote_path('word_100M_compress.txt')} {get_local_path('word_100M_compress_recv.txt')}")


@pytest.mark.repeat(1)
def test_large_file():
    assert check_hdc_cmd(f"file send {get_local_path('large')} {get_remote_path('it_large')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_large')} {get_local_path('large_recv')}")


@pytest.mark.repeat(1)
def test_large_file_compress():
    assert check_hdc_cmd(f"file send -z {get_local_path('large')} {get_remote_path('it_large_z')}")
    assert check_hdc_cmd(f"file recv -z {get_remote_path('it_large_z')} {get_local_path('large_z_recv')}")


@pytest.mark.repeat(1)
def test_running_file():
    assert check_hdc_cmd(f"file recv system/bin/hdcd {get_local_path('running_recv')}")


@pytest.mark.repeat(1)
def test_rate():
    assert check_rate(f"file send {get_local_path('large')} {get_remote_path('it_large')}", 18000)
    assert check_rate(f"file recv {get_remote_path('it_large')} {get_local_path('large_recv')}", 18000)


@pytest.mark.repeat(1)
def test_file_error():
    assert check_hdc_cmd("target mount")
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
    assert check_hdc_cmd(f"shell rm -rf {get_remote_path('../../../large')}")
    assert check_hdc_cmd(f"shell param set persist.hdc.control.file false")
    assert check_shell(
        f"file send {get_local_path('small')} {get_remote_path('it_small_false')}",
        "debugging is not allowed"
    )
    assert check_hdc_cmd(f"shell param set persist.hdc.control.file true")
    assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('it_small_true')}")


@pytest.mark.repeat(5)
def test_recv_dir():
    if os.path.exists(get_local_path('it_problem_dir')):
        rmdir(get_local_path('it_problem_dir'))
    assert check_hdc_cmd(f"shell rm -rf {get_remote_path('it_problem_dir')}")
    assert check_hdc_cmd(f"shell rm -rf {get_remote_path('problem_dir')}")
    assert make_multiprocess_file(get_local_path('problem_dir'), get_remote_path(''), 'send', 1, "dir")
    assert check_hdc_cmd(f"shell mv {get_remote_path('problem_dir')} {get_remote_path('it_problem_dir')}")
    assert make_multiprocess_file(get_local_path(''), get_remote_path('it_problem_dir'), 'recv', 1, "dir")


@pytest.mark.repeat(5)
def test_hap_install():
    assert check_hdc_cmd(f"install -r {get_local_path('entry-default-signed-debug.hap')}",
                            bundle="com.hmos.diagnosis")


@pytest.mark.repeat(5)
def test_install_hap():
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
    assert check_app_install(package_hap, app_name_default, "-s")


@pytest.mark.repeat(5)
def test_install_hsp():
    package_hsp = "libA_v10001.hsp"
    hsp_name_default = "com.example.liba"

    assert check_app_install(package_hsp, hsp_name_default, "-s")
    assert check_app_uninstall(hsp_name_default, "-s")
    assert check_app_install(package_hsp, hsp_name_default)


@pytest.mark.repeat(5)
def test_install_multi_hap():
    # default multi hap
    tables = {
        "entry-default-signed-debug.hap" : "com.hmos.diagnosis",
        "ActsAudioRecorderJsTest.hap" : "ohos.acts.multimedia.audio.audiorecorder"
    }
    assert check_app_install_multi(tables)
    assert check_app_uninstall_multi(tables)
    assert check_app_install_multi(tables, "-s")

    # default multi hap -r -k
    tables = {
        "entry-default-signed-debug.hap" : "com.hmos.diagnosis",
        "ActsAudioRecorderJsTest.hap" : "ohos.acts.multimedia.audio.audiorecorder"
    }
    assert check_app_install_multi(tables, "-r")
    assert check_app_uninstall_multi(tables, "-k")


@pytest.mark.repeat(5)
def test_install_multi_hsp():
    # default multi hsp -s
    tables = {
        "libA_v10001.hsp" : "com.example.liba",
        "libB_v10001.hsp" : "com.example.libb",
    }
    assert check_app_install_multi(tables, "-s")
    assert check_app_uninstall_multi(tables, "-s")
    assert check_app_install_multi(tables)


@pytest.mark.repeat(5)
def test_install_hsp_and_hap():
    #default multi hsp and hsp
    tables = {
        "libA_v10001.hsp" : "com.example.liba",
        "entry-default-signed-debug.hap" : "com.hmos.diagnosis",
    }
    assert check_app_install_multi(tables)
    assert check_app_install_multi(tables, "-s")


@pytest.mark.repeat(5)
def test_install_dir():
    package_haps_dir = "app_dir"
    app_name_default = "com.hmos.diagnosis"
    assert check_app_install(package_haps_dir, app_name_default)
    assert check_app_uninstall(app_name_default)


def test_server_kill():
    assert check_hdc_cmd("kill", "Kill server finish")
    assert check_hdc_cmd("start server")
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    assert check_hdc_cmd("kill -r", "Start server finish")
    assert check_hdc_cmd("start -r", "hdc start server")
    assert check_hdc_cmd("checkserver", "Ver")


def test_target_cmd():
    assert check_hdc_targets()
    check_hdc_cmd("target boot")
    start_time = time.time()
    run_command_with_timeout(f"{GP.hdc_head} wait", 30) # reboot takes up to 30 seconds
    time.sleep(3) # sleep 3s to wait for the device to boot
    run_command_with_timeout(f"{GP.hdc_head} wait", 30) # reboot takes up to 30 seconds
    end_time = time.time()
    print(f"command exec time {end_time - start_time}")
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    assert (end_time - start_time) > 8 # Reboot takes at least 8 seconds


@pytest.mark.repeat(1)
def test_file_switch_off():
    assert check_hdc_cmd("shell param set persist.hdc.control.file false")
    assert check_shell(f"shell param get persist.hdc.control.file", "false")
    assert check_shell(f"file send {get_local_path('small')} {get_remote_path('it_small')}",
                       "debugging is not allowed")
    assert check_shell(f"file recv {get_remote_path('it_small')} {get_local_path('small_recv')}",
                       "debugging is not allowed")


@pytest.mark.repeat(1)
def test_file_switch_on():
    assert check_hdc_cmd("shell param set persist.hdc.control.file true")
    assert check_shell(f"shell param get persist.hdc.control.file", "true")
    assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('it_small')}")
    assert check_hdc_cmd(f"file recv {get_remote_path('it_small')} {get_local_path('small_recv')}")


def test_target_mount():
    assert (check_hdc_cmd("target mount", "Mount finish" or "[Fail]Operate need running as root"))
    remount_vendor = get_shell_result(f'shell "mount |grep /vendor |head -1"')
    print(remount_vendor)
    assert "rw" in remount_vendor
    remount_system = get_shell_result(f'shell "cat proc/mounts | grep /system |head -1"')
    print(remount_system)
    assert "rw" in remount_system


def test_tmode_port():
    assert (check_hdc_cmd("tmode port", "Set device run mode successful"))
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    assert (check_hdc_cmd("tmode port 12345"))
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    netstat_port = get_shell_result(f'shell "netstat -anp | grep 12345"')
    print(netstat_port)
    assert "LISTEN" in netstat_port
    assert "hdcd" in netstat_port


def test_tconn():
    daemon_port = 58710
    address = "127.0.0.1"
    assert (check_hdc_cmd(f"tmode port {daemon_port}"))
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    assert check_hdc_cmd(f"shell param get persist.hdc.port", f"{daemon_port}")
    assert check_hdc_cmd(f"fport tcp:{daemon_port} tcp:{daemon_port}", "Forwardport result:OK")
    assert check_hdc_cmd(f"fport ls ", f"tcp:{daemon_port} tcp:{daemon_port}")
    assert check_shell(f"tconn {address}:{daemon_port}", "Connect OK", head=GP.hdc_exe)
    time.sleep(3)
    assert check_shell("list targets", f"{address}:{daemon_port}", head=GP.hdc_exe)
    tcp_head = f"{GP.hdc_exe} -t {address}:{daemon_port}"
    assert check_hdc_cmd(f"file send {get_local_path('medium')} {get_remote_path('it_tcp_medium')}",
        head=tcp_head)
    assert check_hdc_cmd(f"file recv {get_remote_path('it_tcp_medium')} {get_local_path('medium_tcp_recv')}",
        head=tcp_head)
    assert check_shell(f"tconn {address}:{daemon_port} -remove", head=GP.hdc_exe)
    assert not check_shell("list targets", f"{address}:{daemon_port}", head=GP.hdc_exe)
    assert check_hdc_cmd(f"fport rm tcp:{daemon_port} tcp:{daemon_port}", "Remove forward ruler success")
    assert not check_hdc_cmd(f"fport ls ", f"tcp:{daemon_port} tcp:{daemon_port}")


def test_target_key():
    device_key = get_shell_result(f"list targets").split("\r\n")[0]
    hdcd_pid = get_shell_result(f"-t {device_key} shell pgrep -x hdcd").split("\r\n")[0]
    assert hdcd_pid.isdigit()


def test_version_cmd():
    version = "Ver: 2.0.0a"
    assert check_hdc_version("-v", version)
    assert check_hdc_version("version", version)
    assert check_hdc_version("checkserver", version)


def test_fport_cmd_output():
    local_port = 18070
    remote_port = 11080
    fport_arg = f"tcp:{local_port} tcp:{remote_port}"
    assert check_hdc_cmd(f"fport {fport_arg}", "Forwardport result:OK")
    assert check_shell_any_device(f"netstat -ano", "LISTENING", False)[0]
    assert check_shell_any_device(f"netstat -ano", f"{local_port}", False)[0]
    assert check_hdc_cmd(f"fport ls", fport_arg)
    assert check_hdc_cmd(f"fport rm {fport_arg}", "success")


def test_rport_cmd_output():
    local_port = 17090
    remote_port = 11080
    rport_arg = f"tcp:{local_port} tcp:{remote_port}"
    assert check_hdc_cmd(f"rport {rport_arg}", "Forwardport result:OK")
    netstat_line = get_shell_result(f'shell "netstat -anp | grep {local_port}"')
    assert "LISTEN" in netstat_line
    assert "hdcd" in netstat_line
    fport_list = get_shell_result(f"fport ls")
    assert "Reverse" in fport_list
    assert rport_arg in fport_list
    assert check_hdc_cmd(f"fport rm {rport_arg}", "success")


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
        assert check_hdc_cmd(f"fport rm {rport}", "success")
        assert not check_hdc_cmd("rport ls", fport) and not check_hdc_cmd("fport ls", fport)

    task_str1 = "tcp:33333 tcp:33333"
    assert check_hdc_cmd(f"fport {task_str1}", "Forwardport result:OK")
    assert check_hdc_cmd(f"fport rm {task_str1}", "success")
    assert check_hdc_cmd(f"fport {task_str1}", "Forwardport result:OK")
    assert check_hdc_cmd(f"fport rm {task_str1}", "success")

    task_str2 = "tcp:44444 tcp:44444"
    assert check_hdc_cmd(f"rport {task_str2}", "Forwardport result:OK")
    assert check_hdc_cmd(f"fport rm {task_str2}", "success")
    assert check_hdc_cmd(f"rport {task_str2}", "Forwardport result:OK")
    assert check_hdc_cmd(f"fport rm {task_str2}", "success")


#子进程执行函数
def new_process_run(cmd):
    # 重定向 stdout 和 stderr 到 /dev/null
    with open(os.devnull, 'w') as devnull:
        old_stdout = os.dup2(devnull.fileno(), 1)  # 重定向 stdout
        old_stderr = os.dup2(devnull.fileno(), 2)  # 重定向 stderr
        try:
            # 这里是子进程的代码，不会有任何输出到控制台
            check_shell(f'{cmd}')
        finally:
            # 恢复原始的 stdout 和 stderr
            os.dup2(old_stdout, 1)
            os.dup2(old_stderr, 2)


def test_hilog_exit_after_hdc_kill():
    # 新开进程执行hdc shell hilog，防止阻塞主进程
    p = multiprocessing.Process(target=new_process_run, args=("shell hilog",))
    p.start()
    time.sleep(1)
    hilog_pid = get_shell_result(f'shell pidof hilog')
    hilog_pid = hilog_pid.replace("\r\n", "")
    assert hilog_pid.isdigit()
    assert check_hdc_cmd(f'kill', "Kill server finish")
    assert check_hdc_cmd("start")
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    hilog_pid2 = get_shell_result(f'shell pidof hilog')
    assert hilog_pid2 == ''
    p.join()


def test_shell_cmd_timecost():
    assert check_cmd_time(
        cmd="shell \"ps -ef | grep hdcd\"",
        pattern="hdcd",
        duration=None,
        times=10)


def test_shell_huge_cat():
    assert check_hdc_cmd(f"file send {get_local_path('word_100M.txt')} {get_remote_path('it_word_100M.txt')}")
    assert check_cmd_time(
        cmd=f"shell cat {get_remote_path('it_word_100M.txt')}",
        pattern=None,
        duration=10000, # 10 seconds
        times=10)


def test_file_option_bundle_normal():
    version = "Ver: 3.1.0e"
    if not check_hdc_version("version", version) or not check_hdc_version("shell hdcd -v", version):
        print("version does not match, ignore this case")
    else:
        data_storage_el2_path = "data/storage/el2/base"
        check_shell(f"shell rm -rf mnt/debug/100/debug_hap/{GP.debug_app}/{data_storage_el2_path}/it*")
        check_shell(f"shell mkdir -p mnt/debug/100/debug_hap/{GP.debug_app}/{data_storage_el2_path}")

        # file send & recv test
        test_resource_list = ['empty', 'medium', 'small', 'problem_dir']
        for test_item in test_resource_list:
            if test_item == 'problem_dir' and os.path.exists(get_local_path(f'recv_bundle_{test_item}')):
                rmdir(get_local_path(f'recv_bundle_{test_item}'))

            assert check_hdc_cmd(f"file send -b {GP.debug_app} "
                                 f"{get_local_path(f'{test_item}')} {data_storage_el2_path}/it_{test_item}")
            assert check_hdc_cmd(f"file recv -b {GP.debug_app} {data_storage_el2_path}/it_{test_item} "
                                 f"{get_local_path(f'recv_bundle_{test_item}')} ")


def test_file_option_bundle_error():
    version = "Ver: 3.1.0e"
    if not check_hdc_version("version", version) or not check_hdc_version("shell hdcd -v", version):
        print("version does not match, ignore this case")
    else:
        data_storage_el2_path = "data/storage/el2/base"
        is_user = check_shell("shell id", "uid=2000(shell)")
        assert check_shell(f"file send -b {GP.debug_app} "
                           f"{get_local_path('empty_dir')} {get_remote_path('it_empty_dir')}",
                           "the source folder is empty")

        assert check_shell(f"file send -b {GP.debug_app} ", "There is no local and remote path")
        assert check_shell(f"file recv -b {GP.debug_app} ", "There is no local and remote path")

        # 报错优先级
        assert check_shell(f"file send -b ./{GP.debug_app} ", "There is no local and remote path")
        assert check_shell(f"file recv -b ./{GP.debug_app} ", "There is no local and remote path")

        # 报错优先级
        assert check_shell(f"file recv -b ", "[E005003]")
        assert check_shell(f"file send -b ", "[E005003]")

        assert check_shell(f"file send -b ./{GP.debug_app} "
                    f"{get_local_path('small')} {get_remote_path('it_small')}", "[E005101]")
        assert check_shell(f"file recv -b ./{GP.debug_app} "
                    f"{get_local_path('small')} {get_remote_path('it_small')}", "[E005101]")

        # bundle不存在或不符合要求
        assert check_shell(f"file send -b ./{GP.debug_app} "
                           f"{get_local_path('small')} {get_remote_path('it_small')}", "[E005101]")
        assert check_shell(f"file send -b com.AAAA "
                           f"{get_local_path('small')} {get_remote_path('it_small')}", "[E005101]")
        assert check_shell(f"file send -b 1 "
                           f"{get_local_path('small')} {get_remote_path('it_small')}", "[E005101]")
        assert check_shell(f"file send -b "
                           f"{get_local_path('small')} {get_remote_path('it_small')}", "There is no remote path")
        assert check_shell(f"file recv -b ./{GP.debug_app} "
                           f"{get_remote_path('it_small')} {get_local_path('small_recv')}", "[E005101]")
        # 逃逸
        assert check_shell(f"file send -b {GP.debug_app} "
                           f"{get_local_path('small')} ../../../it_small", "[E005102]")
        assert check_shell(f"file recv -b {GP.debug_app} ../../../it_small"
                           f"{get_local_path('small_recv')} ", "[E005102]")
        assert check_shell(f"file send -b {GP.debug_app} "
                           f"{get_local_path('small')} {data_storage_el2_path}/../../../../../ ",
                           "permission denied" if is_user else "[E005102]")
        assert check_shell(f"file send -b {GP.debug_app} "
                           f"{get_local_path('small')} {data_storage_el2_path}/aa/bb/cc/ ",
                           "[E005102]")


def test_shell_option_bundle_normal():
    version = "Ver: 3.1.0e"
    if not check_hdc_version("version", version) or not check_hdc_version("shell hdcd -v", version):
        print("version does not match, ignore this case")
    else:
        data_storage_el2_path = "data/storage/el2/base"
        check_shell(f"shell mkdir -p mnt/debug/100/debug_hap/{GP.debug_app}/{data_storage_el2_path}")
        assert check_shell(f"shell -b {GP.debug_app} pwd", f"mnt/debug/100/debug_hap/{GP.debug_app}")
        assert check_shell(f"shell -b {GP.debug_app} cd {data_storage_el2_path}; pwd",
                           f"mnt/debug/100/debug_hap/{GP.debug_app}/{data_storage_el2_path}")
        check_shell(f"shell -b {GP.debug_app} touch {data_storage_el2_path}/test01")
        assert not check_shell(f"shell -b {GP.debug_app} touch {data_storage_el2_path}/test01", "denied")
        assert not check_shell(f"shell -b {GP.debug_app} touch -a {data_storage_el2_path}/test01", "denied")
        assert check_shell(f"shell -b {GP.debug_app} ls {data_storage_el2_path}/", "test01")
        assert check_shell(f"shell           -b            {GP.debug_app} echo            123             ",
                    "123")
        check_shell(f"shell -b {GP.debug_app} \"echo 123 > {data_storage_el2_path}/test02\"")
        assert check_shell(f"shell -b {GP.debug_app} cat {data_storage_el2_path}/test02", "123")
        check_shell(f"shell -b {GP.debug_app} mkdir {data_storage_el2_path}/test03")
        assert check_shell(f"shell -b {GP.debug_app} stat {data_storage_el2_path}/test03", "Access")
        check_shell(f"shell -b {GP.debug_app} rm -rf {data_storage_el2_path}/test01 "
                    f"{data_storage_el2_path}/test02 {data_storage_el2_path}/test03")
        assert check_shell(f"shell -b {GP.debug_app} ls {data_storage_el2_path}/test01",
                           "test01: No such file or directory")
        assert check_shell(f"shell -b {GP.debug_app} ls {data_storage_el2_path}/test02",
                           "test02: No such file or directory")
        assert check_shell(f"shell -b {GP.debug_app} ls {data_storage_el2_path}/test03",
                           "test03: No such file or directory")


def test_shell_option_bundle_error():
    version = "Ver: 3.1.0e"
    if not check_hdc_version("version", version) or not check_hdc_version("shell hdcd -v", version):
        print("version does not match, ignore this case")
    else:
        # 不存在的bundle名称
        assert check_shell("shell -b com.XXXX.not.exist.app pwd", "[Fail][E003001]")
        # 相对路径逃逸1
        assert check_shell(f"shell -b ../../../../ pwd", "[Fail][E003001")
        # 相对路径逃逸2
        assert check_shell(f"shell -b ././././pwd", "[Fail][E003001]")
        # 相对路径逃逸3
        assert check_shell(f"shell -b / pwd", "[Fail][E003001]")
        # 交互式shell不支持
        assert check_shell(f"shell -b {GP.debug_app}", "[Fail][E003002]")
        # bundle参数(129)超过128
        str_len_129 = "a" * 129
        assert check_shell(f"shell -b {str_len_129} pwd", "[Fail][E003001]")
        # bundle参数(6)低于7
        str_len_6 = "a" * 6
        assert check_shell(f"shell -b {str_len_6} pwd", "[Fail][E003001]")
        # bundle参数存在非法参数
        str_invalid = "#########"
        assert check_shell(f"shell -b {str_invalid} pwd", "[Fail][E003001]")
        # 其他参数用例1
        assert check_shell("shell -param 1234567890 pwd", "[Fail][E003003]")
        # 其他参数用例2
        assert check_shell("shell -basd {GP.debug_app} pwd", "[Fail][E003003]")
        # 缺少参数字母
        assert check_shell(f"shell - {GP.debug_app} ls", "[Fail][E003003]")
        # 缺少bundle参数1
        assert check_shell("shell -b ls", "[Fail][E003001]")
        # 缺少bundle参数2
        assert check_shell("shell -b", "[Fail][E003005]")
        # 存在未知参数在前面
        assert check_shell(f"shell -t -b {GP.debug_app} ls", "[Fail][E003003]")
        # 参数在后面
        assert check_shell(f"shell ls -b {GP.debug_app}", "No such file or directory")
        # 双倍参数
        assert check_shell(f"shell -b -b {GP.debug_app} ls", "[Fail][E003001]")
        # 双倍-杠
        assert check_shell(f"shell --b {GP.debug_app}", "[Fail][E003003]")


def test_option_bundle_error_usage_support():
    version = "Ver: 3.1.0e"
    if not check_hdc_version("version", version) or not check_hdc_version("shell hdcd -v", version):
        print("version does not match, ignore this case")
    else:
        assert check_shell("shell -b", "Usage: hdc shell [-b bundlename] [COMMAND...]")
        assert check_shell(f"file recv -b ", "Usage: hdc file recv [-b bundlename] remote local")
        assert check_shell(f"file send -b ", "Usage: hdc file send [-b bundlename] local remote")


def test_hdcd_rom():
    baseline = 2200 # 2200KB
    assert check_rom(baseline)


def test_smode_permission():
    check_shell(f"shell rm -rf data/deep_test_dir")
    assert check_hdc_cmd(f'smode -r')
    time.sleep(3)
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    assert check_shell(f"file send {get_local_path('deep_test_dir')} data/",
                       "permission denied")
    assert check_shell(f"file send {get_local_path('deep_test_dir')} data/",
                       "[E005005]")


def test_smode_r():
    assert check_hdc_cmd(f'smode -r')
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    assert check_shell(f"shell id", "context=u:r:sh:s0")
    assert check_shell(f"file send {get_local_path('deep_test_dir')} data/",
                       "permission denied")


def test_smode():
    assert check_hdc_cmd(f'smode')
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    time.sleep(3) # sleep 3s to wait for the device to connect channel
    run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
    assert check_shell(f"shell id", "context=u:r:su:s0")
    assert not check_hdc_cmd("ls /data/log/faultlog/faultlogger | grep hdcd", "hdcd")


def test_persist_hdc_mode_tcp():
    assert check_hdc_cmd(f"shell param set persist.hdc.mode.tcp enable")
    time.sleep(5)
    run_command_with_timeout("hdc wait", 3)
    netstat_listen = get_shell_result(f'shell "netstat -anp | grep 0.0.0.0"')
    assert "LISTEN" in netstat_listen
    assert "hdcd" in netstat_listen
    assert check_hdc_cmd(f"shell param set persist.hdc.mode.tcp disable")
    time.sleep(5)
    run_command_with_timeout("hdc wait", 3)
    netstat_listen = get_shell_result(f'shell "netstat -anp | grep 0.0.0.0"')
    assert "LISTEN" not in netstat_listen
    assert "hdcd" not in netstat_listen


def test_persist_hdc_mode_usb():
    assert check_hdc_cmd(f"shell param set persist.hdc.mode.usb enable")
    echo_result = get_shell_result(f'shell "echo 12345"')
    assert "12345" not in echo_result
    time.sleep(10)
    run_command_with_timeout("hdc wait", 3)
    echo_result = get_shell_result(f'shell "echo 12345"')
    assert "12345" in echo_result


def test_persist_hdc_mode_tcp_usb():
    assert check_hdc_cmd(f"shell param set persist.hdc.mode.tcp enable")
    time.sleep(5)
    run_command_with_timeout("hdc wait", 3)
    assert check_hdc_cmd(f"shell param set persist.hdc.mode.usb enable")
    time.sleep(10)
    run_command_with_timeout("hdc wait", 3)
    netstat_listen = get_shell_result(f'shell "netstat -anp | grep 0.0.0.0"')
    assert "LISTEN" in netstat_listen
    assert "hdcd" in netstat_listen
    assert check_hdc_cmd(f"shell param set persist.hdc.mode.tcp disable")
    time.sleep(5)
    run_command_with_timeout("hdc wait", 3)
    netstat_listen = get_shell_result(f'shell "netstat -anp | grep 0.0.0.0"')
    assert "LISTEN" not in netstat_listen
    assert "hdcd" not in netstat_listen


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

    prepare_source()
    update_source()

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
