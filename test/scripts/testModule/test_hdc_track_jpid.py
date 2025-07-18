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
import pytest
from utils import GP, check_app_install, check_app_uninstall, check_cmd_block, \
    check_hdc_cmd, check_shell, get_shell_result, load_gp, get_end_symbol


class TestHdcTrackJpid:
    bundle_name = "com.example.aacommand07"

    @classmethod
    def setup_class(self):
        package_hap = "AACommand07.hap"
        assert check_app_install(package_hap, self.bundle_name)
        assert check_shell(f"shell aa start -b {self.bundle_name} -a MainAbility")

    @classmethod
    def teardown_class(self):
        assert check_shell(f"shell aa force-stop {self.bundle_name}")
        assert check_app_uninstall(self.bundle_name)

    @pytest.mark.L0
    def test_hdc_jpid(self):
        pidstr = get_shell_result(f'shell "pidof {self.bundle_name}"').split(get_end_symbol())[0]
        assert check_hdc_cmd(f"jpid", pidstr)

    @pytest.mark.L0
    def test_hdc_track_jpid(self):
        pidstr = get_shell_result(f'shell "pidof {self.bundle_name}"').split(get_end_symbol())[0]
        track_cmd_p = f"{GP.hdc_exe} -t {GP.device_name} track-jpid -p"
        pattern_p = f"{pidstr} {self.bundle_name}"
        assert check_cmd_block(track_cmd_p, pattern_p, timeout=2)
        track_cmd_a = f"{GP.hdc_exe} -t {GP.device_name} track-jpid -a"
        pattern_a = f"{pidstr} {self.bundle_name} release"
        assert check_cmd_block(track_cmd_a, pattern_a, timeout=2)
