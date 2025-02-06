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
import pytest
import os
import time
import multiprocessing

from utils import GP, check_hdc_cmd, check_shell, check_version, get_shell_result, run_command_with_timeout, load_gp


class TestShellHilog:
    #子进程执行函数
    def new_process_run(self, cmd):
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
        hilog_pid = hilog_pid.replace("\r\n", "")
        assert hilog_pid.isdigit()
        assert check_hdc_cmd(f'kill', "Kill server finish")
        assert check_hdc_cmd("start")
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
        hilog_pid2 = get_shell_result(f'shell pidof hilog')
        assert hilog_pid2 == ''
        p.join()


class TestShellBundleOption:
    a_long = "a" * 129
    a_short = "a" * 6
    test_bundle_data = [
    ("-b ""com.XXXX.not.exist.app", "pwd", "[Fail][E003001]"),
    ("-b ""../../../../", "pwd", "[Fail][E003001]"),
    ("-b ""././././pwd", "pwd", "[Fail][E003001]"),
    ("-b ""/", "pwd", "[Fail][E003001]"),
    ("-b "f"{GP.debug_app}", "", "[Fail][E003002]"),
    (f"-b {a_long}", "pwd", "[Fail][E003001]"),
    (f"-b {a_short}", "pwd", "[Fail][E003001]"),
    ("-b #########", "pwd", "[Fail][E003001]"),
    ("-param 1234567890", "pwd", "[Fail][E003003]"),
    ("-basd {GP.debug_app}", "pwd", "[Fail][E003003]"),
    ("- {GP.debug_app}", "ls", "[Fail][E003003]"),
    ("-b", "ls", "[Fail][E003001]"),
    ("-b", "", "[Fail][E003005]"),
    ("-t -b {GP.debug_app}", "ls", "[Fail][E003003]"),
    ("ls -b {GP.debug_app}", "", "No such file or directory"),
    ("-b -b {GP.debug_app}", "ls", "[Fail][E003001]"),
    ("--b {GP.debug_app}", "", "[Fail][E003003]"),
    ]

    @classmethod
    def setup_class(self):
        data_storage_el2_path = "data/storage/el2/base"
        check_shell(f"shell mkdir -p mnt/debug/100/debug_hap/{GP.debug_app}/{data_storage_el2_path}")

    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("bundle_option, command, expected_output", test_bundle_data)
    def test_bundle_option_matrix(self, bundle_option, command, expected_output):
        test_command = f"shell {bundle_option} {command}"
        assert check_shell(test_command, expected_output)