#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Huawei Device Co., Ltd.
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


class TestRportMultiDevices:
    devices = []
    rportstr = f"tcp:2000 tcp:5000"

    @classmethod
    def setup_class(self):
        cmd = f"{GP.hdc_exe} list targets -v"
        result, _ = get_cmd_block_output_and_error(cmd)
        result = result.split(f"\n")
        for item in result:
            pattern = f"\tConnected\tlocalhost"
            if pattern in item:
                sn = item.split(f"\t")[0]
                self.devices.append(sn)
        assert len(self.devices) > 1

    @classmethod
    def teardown_class(self):
        cmd = f"{GP.hdc_exe} fport rm {self.rportstr}"
        result, _ = get_cmd_block_output_and_error(cmd)

    def rport_count_of_device(self):       
        count = 0
        result, _ = get_cmd_block_output_and_error(f"{GP.hdc_exe} fport ls")
        for sn in self.devices:
            rstr = f"{sn}    {self.rportstr}    [Reverse]"
            if rstr in result:
                count = count + 1
        return count

    @pytest.mark.L0
    def test_rport_create(self):
        count = 0
        for sn in self.devices:
            cmd = f"{GP.hdc_exe} -t {sn} rport {self.rportstr}"
            result, _ = get_cmd_block_output_and_error(cmd)
            assert f"Forwardport result:OK" in result
            count = count + 1
        assert count == len(self.devices)

    @pytest.mark.L0
    def test_rport_query(self):       
        count = self.rport_count_of_device()
        assert count == len(self.devices)

    @pytest.mark.L0
    def test_rport_delete(self):
        count = self.rport_count_of_device()
        assert count == len(self.devices)
        cmd = f"{GP.hdc_exe} fport rm {self.rportstr}"
        result, _ = get_cmd_block_output_and_error(cmd)
        assert f"Remove forward ruler success" in result
        result, _ = get_cmd_block_output_and_error(f"{GP.hdc_exe} fport ls")
        assert self.rportstr not in result

