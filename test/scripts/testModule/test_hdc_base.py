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
import re
import time
import pytest

from utils import GP, check_hdc_cmd, check_hdc_targets, check_rom, check_hdc_version, get_shell_result, \
     run_command_with_timeout, check_shell, get_remote_path, get_local_path, load_gp


def clear_and_restart():
    check_hdc_cmd("kill")
    check_hdc_cmd("start")
    check_hdc_cmd("wait")
    check_hdc_cmd("shell rm -rf data/local/tmp/it_*")
    check_hdc_cmd("shell rm data/log/faultlog/faultlogger/cppcrash*hdcd*")
    check_hdc_cmd("shell mkdir data/local/tmp/it_send_dir")


class TestListTarget:
    @classmethod
    def setup_class(self):
        clear_and_restart()

    @pytest.mark.L0
    def test_list_targets(self):
        assert check_hdc_targets()
        time.sleep(3)


class TestROM:
    @pytest.mark.L0
    def test_hdcd_rom(self):
        baseline = 2200 # 2200KB
        assert check_rom(baseline)


class TestVersion:
    @pytest.mark.L0
    def test_version_cmd(self):
        version = "Ver: 3.1.0a"
        assert check_hdc_version("-v", version)
        assert check_hdc_version("version", version)

    @pytest.mark.L0
    def test_version_match(self):
        """
        Check that the version numbers of each program are the same
        1. client
        2. server
        3. daemon program
        4. device param
        """
        daemon_param_version = get_shell_result("shell param get const.hdc.version").strip()
        print(f"param version:{daemon_param_version}")
        daemon_version = get_shell_result("shell hdcd -v").strip()
        print(f"daemon version:{daemon_version}")
        client_version = get_shell_result("-v").strip()
        print(f"client version:{client_version}")
        server_version = get_shell_result("version").strip()
        print(f"server version:{server_version}")
        client_server_version = get_shell_result("checkserver").strip()
        print(f"checkserver version:{client_server_version}")

        assert(daemon_param_version == daemon_version)
        assert(daemon_param_version == client_version)
        assert(daemon_param_version == server_version)
        assert(client_server_version.count(daemon_param_version) == 2)


class TestTargetKey:
    @pytest.mark.L0
    def test_target_key(self):
        device_key = re.split("\r|\n", get_shell_result(f"list targets"))[0]
        hdcd_pid = re.split("\r|\n", get_shell_result(f"-t {device_key} shell pgrep -x hdcd"))[0]
        assert hdcd_pid.isdigit()


class TestTargetCommand:
    @pytest.mark.L0
    def test_target_cmd(self):
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

    @pytest.mark.L0
    def test_target_mount(self):
        assert (check_hdc_cmd("target mount", "Mount finish" or "[Fail]Operate need running as root"))
        is_linux = check_hdc_cmd("shell uname", "Linux")
        sep = "/"
        remount_vendor = get_shell_result(f'shell "mount |grep {sep}vendor |head -1"')
        print(remount_vendor)
        if is_linux:
            assert "rw" in remount_vendor
        else:
            assert "ro" in remount_vendor
        remount_system = get_shell_result(f'shell "cat proc/mounts | grep {sep}system |head -1"')
        print(remount_system)
        assert "rw" in remount_system


class TestSwitch:
    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_file_switch_off(self):
        assert check_hdc_cmd("shell param set persist.hdc.control.file false")
        assert check_shell(f"shell param get persist.hdc.control.file", "false")
        assert check_shell(f"file send {get_local_path('small')} {get_remote_path('it_small')}",
                        "debugging is not allowed")
        assert check_shell(f"file recv {get_remote_path('it_small')} {get_local_path('small_recv')}",
                        "debugging is not allowed")

    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_file_switch_on(self):
        assert check_hdc_cmd("shell param set persist.hdc.control.file true")
        assert check_shell(f"shell param get persist.hdc.control.file", "true")
        assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('it_small')}")
        assert check_hdc_cmd(f"file recv {get_remote_path('it_small')} {get_local_path('small_recv')}")