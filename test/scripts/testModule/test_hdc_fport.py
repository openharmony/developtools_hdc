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
from utils import GP, check_shell_any_device, check_hdc_cmd, get_shell_result, load_gp


class TestXportCommand:
    @pytest.mark.L0
    def test_fport_cmd_output(self):
        local_port = 18070
        remote_port = 11080
        fport_arg = f"tcp:{local_port} tcp:{remote_port}"
        assert check_hdc_cmd(f"fport {fport_arg}", "Forwardport result:OK")
        assert check_shell_any_device(f"netstat -ano", "LISTENING", False)[0]
        assert check_shell_any_device(f"netstat -ano", f"{local_port}", False)[0]
        assert check_hdc_cmd(f"fport ls", fport_arg)
        assert check_hdc_cmd(f"fport rm {fport_arg}", "success")

    @pytest.mark.L0
    def test_rport_cmd_output(self):
        local_port = 17090
        remote_port = 11080
        rport_arg = f"tcp:{local_port} tcp:{remote_port}"
        assert check_hdc_cmd(f"rport {rport_arg}", "Forwardport result:OK")
        netstat_line = get_shell_result(f'shell "netstat -anp | grep {local_port}"')
        assert "LISTEN" in netstat_line
        assert "hdcd" in netstat_line
        fport_list = get_shell_result(f"fport ls")
        assert "Reverse" in fport_list
        assert rport_arg in fport_list
        assert check_hdc_cmd(f"fport rm {rport_arg}", "success")

    @pytest.mark.L0
    def test_fport_cmd(self):
        fport_list = []
        rport_list = []
        start_port = 10000
        end_port = 10020
        for i in range(start_port, end_port):
            fport = f"tcp:{i+100} tcp:{i+200}"
            rport = f"tcp:{i+300} tcp:{i+400}"
            localabs = f"tcp:{i+500} localabstract:{f'helloworld.com.app.{i+600}'}"
            fport_list.append(fport)
            rport_list.append(rport)
            fport_list.append(localabs)

        for fport in fport_list:
            assert check_hdc_cmd(f"fport {fport}", "Forwardport result:OK")
            assert check_hdc_cmd(f"fport {fport}", "TCP Port listen failed at")
            assert check_hdc_cmd("fport ls", fport)

        for fport in fport_list:
            assert check_hdc_cmd(f"fport rm {fport}", "success")
            assert not check_hdc_cmd("fport ls", fport)

        for rport in rport_list:
            assert check_hdc_cmd(f"rport {rport}", "Forwardport result:OK")
            assert check_hdc_cmd(f"rport {rport}", "TCP Port listen failed at")
            assert check_hdc_cmd("rport ls", rport) or check_hdc_cmd("fport ls", rport)

        for rport in rport_list:
            assert check_hdc_cmd(f"fport rm {rport}", "success")
            assert not check_hdc_cmd("rport ls", fport) and not check_hdc_cmd("fport ls", fport)

        task_str1 = "tcp:33333 tcp:33333"
        assert check_hdc_cmd(f"fport {task_str1}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport rm {task_str1}", "success")
        assert check_hdc_cmd(f"fport {task_str1}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport rm {task_str1}", "success")

        task_str2 = "tcp:44444 tcp:44444"
        assert check_hdc_cmd(f"rport {task_str2}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport rm {task_str2}", "success")
        assert check_hdc_cmd(f"rport {task_str2}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport rm {task_str2}", "success")
