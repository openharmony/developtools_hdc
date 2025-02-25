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
import time
import pytest
from utils import GP, check_hdc_cmd, get_shell_result, run_command_with_timeout, load_gp


class TestPersistMode:
    @pytest.mark.L0
    def test_persist_hdc_mode_tcp(self):
        assert check_hdc_cmd(f"shell param set persist.hdc.mode.tcp enable")
        time.sleep(5)
        run_command_with_timeout("hdc wait", 3)
        netstat_listen = get_shell_result(f'shell "netstat -anp | grep tcp | grep hdcd"')
        assert "LISTEN" in netstat_listen
        assert "hdcd" in netstat_listen
        assert check_hdc_cmd(f"shell param set persist.hdc.mode.tcp enable")
        time.sleep(5)
        run_command_with_timeout("hdc wait", 3)
        netstat_listen = get_shell_result(f'shell "netstat -anp | grep tcp | grep hdcd"')
        assert "LISTEN" in netstat_listen
        assert "hdcd" in netstat_listen
        assert check_hdc_cmd(f"shell param set persist.hdc.mode.tcp disable")
        time.sleep(5)
        run_command_with_timeout("hdc wait", 3)
        netstat_listen = get_shell_result(f'shell "netstat -anp | grep tcp | grep hdcd"')
        assert "LISTEN" not in netstat_listen
        assert "hdcd" not in netstat_listen

    @pytest.mark.L0
    def test_persist_hdc_mode_usb(self):
        assert check_hdc_cmd(f"shell param set persist.hdc.mode.usb enable")
        echo_result = get_shell_result(f'shell "echo 12345"')
        assert "12345" not in echo_result
        time.sleep(10)
        run_command_with_timeout("hdc wait", 3)
        echo_result = get_shell_result(f'shell "echo 12345"')
        assert "12345" in echo_result

    @pytest.mark.L0
    def test_persist_hdc_mode_tcp_usb(self):
        assert check_hdc_cmd(f"shell param set persist.hdc.mode.tcp enable")
        time.sleep(5)
        run_command_with_timeout("hdc wait", 3)
        assert check_hdc_cmd(f"shell param set persist.hdc.mode.usb enable")
        time.sleep(10)
        run_command_with_timeout("hdc wait", 3)
        netstat_listen = get_shell_result(f'shell "netstat -anp | grep tcp | grep hdcd"')
        assert "LISTEN" in netstat_listen
        assert "hdcd" in netstat_listen
        assert check_hdc_cmd(f"shell param set persist.hdc.mode.tcp disable")
        time.sleep(5)
        run_command_with_timeout("hdc wait", 3)
        netstat_listen = get_shell_result(f'shell "netstat -anp | grep tcp | grep hdcd"')
        assert "LISTEN" not in netstat_listen
        assert "hdcd" not in netstat_listen


class TestTmodeCommand:
    @pytest.mark.L0
    def test_tmode_port(self):
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