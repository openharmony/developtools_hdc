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
from utils import GP, check_hdc_cmd, load_gp, get_cmd_block_output_and_error


class TestHdcHelp:
    @pytest.mark.L0
    def test_hdc_help(self):
        _, error = get_cmd_block_output_and_error(f"{GP.hdc_head} -h")
        assert "Generate public" in error

        assert check_hdc_cmd("help", "Generate public")

    @pytest.mark.L0
    def test_hdc_help_verbose_heartbeat(self):
        _, error = get_cmd_block_output_and_error(f"{GP.hdc_head} -h verbose")
        assert "OHOS_HDC_HEARTBEAT" in error

        assert check_hdc_cmd("help verbose", "OHOS_HDC_HEARTBEAT")
