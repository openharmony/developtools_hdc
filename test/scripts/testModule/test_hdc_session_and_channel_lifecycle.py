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
import os
import time
import logging
import multiprocessing
import platform
import pytest
import tempfile

from utils import GP, check_shell, run_command_with_timeout, get_cmd_block_output, get_shell_result, get_end_symbol, load_gp

logger = logging.getLogger(__name__)


class TestSessionLifeCycle:

    def server_run_method(self, cmd):
        get_cmd_block_output(cmd, timeout=30)

    def remove(self, source_line, target_list):
        for item in target_list:
            if source_line.find(item) >= 0:
                target_list.remove(item)
                return (True, target_list)
        return (False, target_list)

    @pytest.mark.L0
    def test_session_lifecycle_by_reboot(self):
        check_shell(f"kill")
        time.sleep(3)

        cmd = f"hdc -m"
        p = multiprocessing.Process(target=self.server_run_method, args=(cmd,))
        p.start()

        run_command_with_timeout(f"{GP.hdc_head} wait", 20)

        time.sleep(3)
        devices = get_shell_result(f"list targets").split(get_end_symbol())
        target_lines = list()
        for item in devices:
            if len(item) > 0:
                connect_key = f"{item[:3]}******{item[-3:]}(L:{len(item)})"
                line = f"connectKey:{connect_key} connType:0 connect state:1 faultInfo:"
                target_lines.append(line)
                line = f"connectKey:{connect_key} connType:0 connect state:0 faultInfo:LIBUSB_TRANSFER_"
                target_lines.append(line)
        time.sleep(3)
        check_shell(f"shell reboot")
        time.sleep(5)
        check_shell(f"kill")
        is_ohos = "Harmony" in platform.system()
        # 检查serverOutput是否包含上述字符串
        if not is_ohos:
            tmp_path = tempfile.gettempdir()
        else:
            tmp_path = os.path.expanduser("~")
        hdc_log_path = f"{tmp_path}{os.sep}hdc.log"
        with open(hdc_log_path, 'r') as file:
            for line in file.readlines():
                (_ret, _target_lines1) = self.remove(line, target_lines)
                target_lines = _target_lines1
        p.join()
        assert len(target_lines) == 0

    @pytest.mark.L0
    def test_session_lifecycle_of_multi_tcp(self):
        check_shell(f"kill")
        time.sleep(3)
        p = multiprocessing.Process(target=self.server_run_method, args=(f"hdc -m",))
        p.start()

        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        fport_tcp_port = 27772
        fport_tcp_host_port = 20001
        assert check_shell(f"tmode port {fport_tcp_port}", "Set device run mode successful")
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        for i in range(4):
            assert check_shell(f"fport tcp:{fport_tcp_host_port + i} tcp:{fport_tcp_port}", "Forwardport result:OK")
        time.sleep(3)
        for i in range(4):
            assert check_shell(f"tconn 127.0.0.1:{fport_tcp_host_port + i}", "Connect OK")
        devices = get_shell_result(f"list targets").split(get_end_symbol())
        target_lines = list()
        for item in devices:
            if len(item) > 0 and item.find(".") >= 0:
                connect_key = f"{item[:3]}******{item[-3:]}(L:{len(item)})"
                line = f"connectKey:{connect_key} connType:1 connect state:1 faultInfo:"
                target_lines.append(line)
                line = f"connectKey:{connect_key} connType:1 connect state:0 faultInfo:end of file"
                if item == f"127.0.0.1:{fport_tcp_host_port + 3}":
                    line = line + " commandCount:3"
                target_lines.append(line)

        time.sleep(1)
        item = f"127.0.0.1:{fport_tcp_host_port + 3}"
        connect_key = f"{item[:3]}******{item[-3:]}(L:{len(item)})"
        ls_result_line = f"connectKey:{connect_key} command flag:1001 command parameters:ls command result:1 command take time:"
        for i in range(3):
            target_lines.append(ls_result_line)
            check_shell(f"-t 127.0.0.1:{fport_tcp_host_port + 3} shell ls")

        for i in range(4):
            assert check_shell(f"fport rm tcp:{fport_tcp_host_port + i} tcp:{fport_tcp_port}",
                               "Remove forward ruler success")
        time.sleep(5)
        check_shell(f"kill")
        is_ohos = "Harmony" in platform.system()
        # 检查serverOutput是否包含上述字符串
        if not is_ohos:
            tmp_path = tempfile.gettempdir()
        else:
            tmp_path = os.path.expanduser("~")
        hdc_log_path = f"{tmp_path}{os.sep}hdc.log"
        with open(hdc_log_path, 'r') as file:
            for line in file.readlines():
                (_ret, _target_lines1) = self.remove(line, target_lines)
                target_lines = _target_lines1

        p.join()
        assert len(target_lines) == 0