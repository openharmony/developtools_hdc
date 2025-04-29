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
import os
import time
import multiprocessing
import logging
import pytest

from utils import GP, check_hdc_cmd, check_shell, check_version, get_shell_result, run_command_with_timeout, load_gp, \
    get_hdcd_pss, get_end_symbol


logger = logging.getLogger(__name__)


class TestShellHilog:
    #子进程执行函数
    @staticmethod
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

    @pytest.mark.L0
    def test_hilog_exit_after_hdc_kill(self):
        # 新开进程执行hdc shell hilog，防止阻塞主进程
        p = multiprocessing.Process(target=self.new_process_run, args=("shell hilog",))
        p.start()
        time.sleep(1)
        hilog_pid = get_shell_result(f'shell pidof hilog')
        hilog_pid = hilog_pid.replace(get_end_symbol(), "")
        assert hilog_pid.isdigit()
        assert check_hdc_cmd(f'kill', "Kill server finish")
        assert check_hdc_cmd("start")
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
        hilog_pid2 = get_shell_result(f'shell pidof hilog')
        assert hilog_pid2 == ''
        p.join()


class TestShellBundleOption:
    pss = 0
    a_long = "a" * 129
    a_short = "a" * 8
    data_storage_el2_path = "data/storage/el2/base"
    test_bundle_fail_data = [
        ("bundle name unknown", "-b ""com.XXXX.not.exist.app", "pwd", "[Fail][E003001]"),
        ("bundle name with path ../", "-b ""../../../../", "pwd", "[Fail][E003001]"),
        ("bundle name with path ./", "-b ""././././pwd", "pwd", "[Fail][E003001]"),
        ("bundle name with path /", "-b ""/", "pwd", "[Fail][E003001]"),
        ("bundle name without command", "-b "f"{GP.debug_app}", "", "[Fail][E003002]"),
        ("bundle name too long: length > 128", f"-b {a_long}", "pwd", "[Fail][E003001]"),
        ("bundle name too short: length < 9", f"-b {a_short}", "pwd", "[Fail][E003001]"),
        ("bundle name with unsupport symbol: #", "-b #########", "pwd", "[Fail][E003001]"),
        ("option: -param", "-param 1234567890", "pwd", "[Fail][E003003]"),
        ("option: -basd", "-basd {GP.debug_app}", "pwd", "[Fail][E003003]"),
        ("parameter missing", "- {GP.debug_app}", "ls", "[Fail][E003003]"),
        ("bundle name missing", "-b", "ls", "[Fail][E003001]"),
        ("bundle name & command missing", "-b", "", "[Fail][E003005]"),
        ("option: -t -b", "-t -b {GP.debug_app}", "ls", "[Fail][E003003]"),
        ("command with similar parameter: ls -b", "ls -b {GP.debug_app}", "", "No such file or directory"),
        ("option: -b -b", "-b -b {GP.debug_app}", "ls", "[Fail][E003001]"),
        ("option: --b", "--b {GP.debug_app}", "", "[Fail][E003003]"),
    ]

    test_bundle_normal_data = [
        ("mkdir", f"shell mkdir -p mnt/debug/100/debug_hap/{GP.debug_app}/{data_storage_el2_path}", None, True),
        ("pwd", f"shell -b {GP.debug_app} pwd", f"mnt/debug/100/debug_hap/{GP.debug_app}", True),
        ("cd_pwd", f"shell -b {GP.debug_app} cd {data_storage_el2_path}; pwd",
            f"mnt/debug/100/debug_hap/{GP.debug_app}/{data_storage_el2_path}", True),
        ("touch", f"shell -b {GP.debug_app} touch {data_storage_el2_path}/test01", None, True),
        ("touch_denied", f"shell -b {GP.debug_app} touch {data_storage_el2_path}/test01", "denied", False),
        ("touch_a_denied", f"shell -b {GP.debug_app} touch -a {data_storage_el2_path}/test01", "denied", False),
        ("ls_test01", f"shell -b {GP.debug_app} ls {data_storage_el2_path}/", "test01", True),
        ("echo_123", f"shell -b {GP.debug_app} echo 123", "123", True),
        ("echo_to_test02", f"shell -b {GP.debug_app} \"echo 123 > {data_storage_el2_path}/test02\"", None, True),
        ("cat_test02", f"shell -b {GP.debug_app} cat {data_storage_el2_path}/test02", "123", True),
        ("mkdir_test03", f"shell -b {GP.debug_app} mkdir {data_storage_el2_path}/test03", None, True),
        ("stat_test03", f"shell -b {GP.debug_app} stat {data_storage_el2_path}/test03", "Access", True),
        ("rm_rf", f"shell -b {GP.debug_app} rm -rf {data_storage_el2_path}/test01 "
            f"{data_storage_el2_path}/test02 {data_storage_el2_path}/test03", None, True),
        ("ls_test01_not_exist", f"shell -b {GP.debug_app} ls {data_storage_el2_path}/test01",
            "test01: No such file or directory", True),
        ("ls_test02_not_exist", f"shell -b {GP.debug_app} ls {data_storage_el2_path}/test02",
            "test02: No such file or directory", True),
        ("ls_test03_not_exist", f"shell -b {GP.debug_app} ls {data_storage_el2_path}/test03",
            "test03: No such file or directory", True),
    ]

    def setup_class(self):
        data_storage_el2_path = "data/storage/el2/base"
        check_shell(f"shell mkdir -p mnt/debug/100/debug_hap/{GP.debug_app}/{data_storage_el2_path}")
        check_shell(f"shell rm -rf -p mnt/debug/100/debug_hap/{GP.debug_app}/{data_storage_el2_path}/it_*")
        self.pss = get_hdcd_pss()
        if self.pss == 0:
            logger.error("get hdcd mem pss failed")


    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("test_name, bundle_option, command, expected_output", test_bundle_fail_data,
                             ids=[name for name, _, _, _ in test_bundle_fail_data])
    def test_bundle_option_error(self, test_name, bundle_option, command, expected_output):
        test_command = f"shell {bundle_option} {command}"
        assert check_shell(test_command, expected_output)

    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("test_name, command, expected_output, assert_bool", test_bundle_normal_data,
                             ids=[name for name, _, _, _ in test_bundle_normal_data])
    def test_shell_option_bundle_normal(self, test_name, command, expected_output, assert_bool):
        if assert_bool:
            assert check_shell(f"{command}", expected_output)
        else:
            assert not check_shell(f"{command}", expected_output)
    
    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    def test_shell_pss_leak(self):
        pss_now = get_hdcd_pss()
        if self.pss == 0 or pss_now == 0:
            logger.error("get hdcd mem pss failed")
            assert False
        if pss_now > (self.pss + 50):
            logger.warning("hdcd mem pss leak, original value %d, now value %d", self.pss, pss_now)
            assert False


class TestShellNormalFuction:
    end_symbol_data = get_end_symbol()
    test_bundle_fail_data = [
        ("shell echo test1", "shell echo test", f"test{end_symbol_data}", True),
        ("shell echo test2", "shell echo 测试", f"测试{end_symbol_data}", True),
        ("shell echo test3", "shell echo test 测试", f"test 测试{end_symbol_data}", True),
    ]

    @pytest.mark.L0
    @pytest.mark.parametrize("test_name, command, expected_output, assert_bool", test_bundle_fail_data,
                             ids=[name for name, _, _, _ in test_bundle_fail_data])
    def test_shell_end(self, test_name, command, expected_output, assert_bool):
        if assert_bool:
            assert check_shell(f"{command}", expected_output)
        else:
            assert not check_shell(f"{command}", expected_output)