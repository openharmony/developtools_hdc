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
from utils import GP, check_cmd_time, check_hdc_cmd, check_rate, get_local_path, get_remote_path, load_gp


def clear_env():
    check_hdc_cmd("shell rm -rf data/local/tmp/it_*")
    check_hdc_cmd("shell mkdir data/local/tmp/it_send_dir")


class TestShellPerformance:
    @pytest.mark.L0
    def test_shell_cmd_timecost(self):
        assert check_cmd_time(
            cmd="shell \"ps -ef | grep hdcd\"",
            pattern="hdcd",
            duration=None,
            times=10)

    @pytest.mark.L0
    def test_shell_huge_cat(self):
        assert check_hdc_cmd(f"file send {get_local_path('word_100M.txt')} {get_remote_path('it_word_100M.txt')}")
        assert check_cmd_time(
            cmd=f"shell cat {get_remote_path('it_word_100M.txt')}",
            pattern=None,
            duration=10000, # 10 seconds
            times=10)


class TestFilePerformance:
    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_rate(self):
        clear_env()
        assert check_rate(f"file send {get_local_path('large')} {get_remote_path('it_large')}", 18000)
        assert check_rate(f"file recv {get_remote_path('it_large')} {get_local_path('large_recv')}", 18000)
    