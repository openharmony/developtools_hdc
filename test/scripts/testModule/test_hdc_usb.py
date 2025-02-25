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
from utils import GP, check_hdc_cmd, check_hdc_targets, check_shell_any_device, load_gp


class TestUsbConnect:
    @pytest.mark.L0
    def test_usb_disconnect(self):
        # 注意：断连后需要等待一段时间才能重新连接
    
        assert check_hdc_targets()
        cmd = 'shell "kill -9 `pidof hdcd`"'
        check_hdc_cmd(f"{cmd}", "[Fail][E001003] USB communication abnormal, please check the USB communication link.")
        time.sleep(3)
        assert check_hdc_targets()

    @pytest.mark.L0
    def test_list_targets_multi_usb_device(self):
        devices_str = check_shell_any_device(f"{GP.hdc_exe} list targets", None, True)
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        devices_array = devices_str[1].split('\n')
        if devices_array:
            for device in devices_array:
                if len(device) > 8:
                    assert check_shell_any_device(f"{GP.hdc_exe} -t {device} shell id", "u:r:")[0]
