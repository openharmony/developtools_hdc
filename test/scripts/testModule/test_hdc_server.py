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
import os
import time
import pytest
from utils import GP, check_cmd_block, check_hdc_cmd, load_gp


class TestHdcServer:
    @pytest.mark.L0
    def test_hdc_server_foreground(self):
        port = os.getenv('OHOS_HDC_SERVER_PORT')
        if port is None:
            port = 8710
        assert check_hdc_cmd("kill", "Kill server finish")
        assert check_cmd_block(f"{GP.hdc_exe} -m", f"port: {port}", timeout=5)
        assert check_hdc_cmd("start")
        time.sleep(3) # sleep 3s to wait for the device to connect channel

    @pytest.mark.L0
    def test_server_kill(self):
        assert check_hdc_cmd("kill", "Kill server finish")
        assert check_hdc_cmd("start server")
        time.sleep(3) # sleep 3s to wait for the device to connect channel
        assert check_hdc_cmd("kill -r", "Start server finish")
        assert check_hdc_cmd("start -r", "hdc start server")
        assert check_hdc_cmd("checkserver", "Ver")
        time.sleep(3) # sleep 3s to wait for the device to connect channel