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
from utils import GP, get_cmd_block_output_and_error


class TestListTargetsOfUsb:
    usbs = []

    @classmethod
    def setup_class(self):
        cmd = f"{GP.hdc_exe} list targets -v"
        result, _ = get_cmd_block_output_and_error(cmd)
        print(result)
        result = result.split(f"\n")
        for item in result:
            pattern = f"USB\tConnected\tlocalhost"
            if pattern in item:
                sn = item.split(f"\t")[0]
                self.usbs.append(sn)
        assert len(self.usbs) > 0

    @classmethod
    def teardown_class(self):
        pass

    @pytest.mark.L0
    def test_shell_id(self):
        count = 0
        for sn in self.usbs:
            cmd = f"{GP.hdc_exe} -t {sn} shell id"
            result, _ = get_cmd_block_output_and_error(cmd)
            print(result)
            assert result.find(f"uid=") >= 0
            count = count + 1
        assert count == len(self.usbs)

    @pytest.mark.L0
    def test_shell_ls(self):
        count = 0
        for sn in self.usbs:
            cmd = f"{GP.hdc_exe} -t {sn} shell ls"
            result, _ = get_cmd_block_output_and_error(cmd)
            print(result)
            count = count + 1
        assert count == len(self.usbs)

    @pytest.mark.L0
    def test_shell_no_such_file(self):
        count = 0
        for sn in self.usbs:
            cmd = f"{GP.hdc_exe} -t {sn} find /system -name nosuchfile"
            result, _ = get_cmd_block_output_and_error(cmd)
            print(result)
            count = count + 1
        assert count == len(self.usbs)

