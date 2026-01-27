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
import platform
import pytest
from utils import GP, check_hdc_cmd, get_shell_result, run_command_with_timeout, check_shell, get_local_path, \
    get_remote_path, rmdir, load_gp, check_cmd_block


class TestTcpByFport:
    daemon_port = 58710
    address = "127.0.0.1"
    fport_args = f"tcp:{daemon_port} tcp:{daemon_port}"
    base_file_table = [
        ("empty", "it_empty"),
        ("small", "it_small"),
        ("medium", "it_medium"),
        ("word_100M.txt", "it_word_100M"),
        ("problem_dir", "it_problem_dir")
    ]

    @classmethod
    def setup_class(self):
        assert (check_hdc_cmd(f"tmode port {self.daemon_port}"))
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
        assert check_hdc_cmd(f"shell param get persist.hdc.port", f"{self.daemon_port}")
        assert check_hdc_cmd(f"fport {self.fport_args}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport ls ", self.fport_args)
        assert check_shell(f"tconn {self.address}:{self.daemon_port}", "Connect OK", head=GP.hdc_exe)
        time.sleep(3)
        assert check_shell("list targets", f"{self.address}:{self.daemon_port}", head=GP.hdc_exe)


    @pytest.mark.L1
    @pytest.mark.parametrize("local_path, remote_path", base_file_table)
    def test_fport_file(self, local_path, remote_path):
        tcp_head = f"{GP.hdc_exe} -t {self.address}:{self.daemon_port}"
        assert check_hdc_cmd(f"file send {get_local_path(local_path)} "
                             f"{get_remote_path(f'{remote_path}_fport')}", head=tcp_head)
        assert check_hdc_cmd(f"file recv {get_remote_path(f'{remote_path}_fport')} "
                             f"{get_local_path(f'{local_path}_fport_recv')}", head=tcp_head)

    @pytest.mark.L1
    def test_no_install_with_multi_device(self):
        package_hap = "AACommand07.hap"
        app = os.path.join(GP.local_path, package_hap)
        install_cmd = f"install {app}"
        patten = f"please confirm a device by help info"
        assert check_cmd_block(f"{GP.hdc_exe} {install_cmd}", f"{patten}", timeout=3)

    @pytest.mark.L1
    def test_remote_tcp_connect(self):
        is_ohos = "Harmony" in platform.system()
        if not is_ohos:
            assert True
            return
        pid = os.getpid()
        is_hishell = check_shell(f'ps -efZ | grep {pid}', 'u:r:hishell_hap:s0')
        check_hdc_cmd("kill")
        cmd = "-s 0.0.0.0:8710 -m"
        if is_hishell:
            assert check_hdc_cmd(cmd, "Program running. Ver:")
        else:
            assert check_hdc_cmd(cmd, "[E001105] Unsupport option [s], please try command in HiShell.")

    @pytest.mark.L1
    def test_tconn_connect(self):
        check_shell(f"kill")
        time.sleep(3)
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)

        assert check_shell(f"tmode port 7777", "Set device run mode successful.")
        time.sleep(2)
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        assert check_shell(f"fport tcp:6666 tcp:7777", "Forwardport result:OK")
        assert check_shell(f"tconn 127.0.0.1:6666", "Connect OK")

        assert check_shell(f"-t 127.0.0.1:6666 shell id", "uid=0(root)")
        result = get_shell_result(f"-t 127.0.0.1:6666 shell ls")
        
        assert len(result.split("\n")) > 5

