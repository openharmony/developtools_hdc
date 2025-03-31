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
from utils import GP, check_hdc_cmd, check_shell, get_local_path, run_command_with_timeout, load_gp


class TestSmode:
    @pytest.mark.L0
    def test_smode_permission(self):
        check_shell(f"shell rm -rf data/deep_test_dir")
        assert check_hdc_cmd(f'smode -r')
        time.sleep(3)
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
        assert check_shell(f"file send {get_local_path('deep_test_dir')} data/",
                        "permission denied")
        assert check_shell(f"file send {get_local_path('deep_test_dir')} data/",
                        "[E005005]")

    @pytest.mark.L0
    def test_smode_r(self):
        assert check_hdc_cmd(f'smode -r')
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
        assert check_shell(f"shell id", "context=u:r:sh:s0")

    @pytest.mark.L0
    def test_smode(self):
        assert check_hdc_cmd(f'smode')
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # wait 3s for the device to connect channel
        assert check_shell(f"shell id", "context=u:r:su:s0")
        assert not check_hdc_cmd("ls data/log/faultlog/faultlogger | grep hdcd", "hdcd")