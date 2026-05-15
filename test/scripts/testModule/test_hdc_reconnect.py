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
from utils import GP, check_shell_any_device, check_shell, load_gp


class TestHdcReconnect:
    @pytest.mark.L0
    def test_reconnect_no_target_key(self):
        assert check_shell("reconnect", "[Fail]Usage: reconnect <target-key>", head=GP.hdc_exe)

    @pytest.mark.L0
    def test_reconnect_nonexistent_target(self):
        assert check_shell("reconnect fake_serial_not_exist",
                           "[Fail]Target device fake_serial_not_exist not available", head=GP.hdc_exe)

    @pytest.mark.L0
    def test_reconnect_tcp_target_rejected(self):
        """Reconnect should reject non-USB (TCP) targets."""
        # Connect a TCP target first
        result = check_shell("tconn 127.0.0.1:10178", fetch=True, head=GP.hdc_exe)
        if "Connect OK" not in result:
            pytest.skip("No TCP daemon available for test")
        assert check_shell("reconnect 127.0.0.1:10178",
                           "[Fail]Reconnect only supports USB devices", head=GP.hdc_exe)
        # cleanup
        check_shell("tconn 127.0.0.1:10178 -remove", head=GP.hdc_exe)

    @pytest.mark.L0
    def test_reconnect_usb_device(self):
        """Reconnect a USB device and verify it comes back."""
        output = check_shell("list targets", fetch=True, head=GP.hdc_exe)
        if "[Empty]" in output:
            pytest.skip("No USB device connected")

        # Get the first USB target
        targets = [t.strip() for t in output.strip().split('\n') if t.strip()]
        usb_target = None
        for t in targets:
            # TCP targets contain ':' (ip:port), USB targets are serial numbers
            if ':' not in t:
                usb_target = t.split()[0] if ' ' in t else t
                break
        if usb_target is None:
            pytest.skip("No USB target found in list targets")

        # Reconnect
        assert check_shell(f"reconnect {usb_target}", "Reconnecting", head=GP.hdc_exe)

        # Wait for device to come back
        check_shell_any_device(f"{GP.hdc_exe} wait", None, False)
        time.sleep(2)

        # Verify device is back
        assert check_shell("list targets", usb_target, head=GP.hdc_exe)
