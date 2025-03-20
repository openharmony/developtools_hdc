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
    check_hdc_cmd, check_shell


class TestHdcTrackJpid:
    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_hdc_jpid(self):
        assert check_hdc_cmd(f"jpid")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_hdc_track_jpid(self):
        package_hap = "AACommand07.hap"
        bundle_name = "com.example.aacommand07"
        assert check_app_install(package_hap, bundle_name)
        assert check_shell(f"shell aa start -b {bundle_name} -a MainAbility")
        assert check_cmd_block(f"{GP.hdc_exe} track-jpid -p", f"{bundle_name}", timeout=2)
        assert check_cmd_block(f"{GP.hdc_exe} track-jpid -a", f"{bundle_name}", timeout=2)
        assert check_app_uninstall(bundle_name)
