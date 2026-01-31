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
import platform
import pytest
import subprocess
import tempfile
from utils import GP, check_cmd_block, check_hdc_cmd, load_gp, get_server_pid_from_file

def get_hdc_server_pid():
    result = subprocess.run('tasklist | findstr hdc', shell=True, capture_output=True, text=True).stdout
    if result.strip():
        return result.split()[1]
    return None


def del_server_pid_file():
    is_ohos = "Harmony" in platform.system()
    if not is_ohos:
        tmp_dir_path = tempfile.gettempdir()
    else:
        tmp_dir_path = os.path.expanduser("~")
    pid_file = os.path.join(tmp_dir_path, ".HDCServer.pid")
    os.remove(pid_file)


class TestHdcServer:
    @pytest.mark.L0
    def test_hdc_server_foreground(self):
        port = os.getenv('OHOS_HDC_SERVER_PORT')
        if port is None:
            port = 8710
        assert check_hdc_cmd("start")
        assert check_hdc_cmd("-l5 kill", "Kill server finish")
        assert check_cmd_block(f"{GP.hdc_exe} -m", f"port: {port}", timeout=5)
        assert check_hdc_cmd("start")
        time.sleep(3) # sleep 3s to wait for the device to connect channel

    @pytest.mark.L0
    def test_server_kill(self):
        assert check_hdc_cmd("start")
        pid_first = get_server_pid_from_file()
        assert check_hdc_cmd("-l5 kill", "Kill server finish")
        assert not check_hdc_cmd("start", "start server")
        time.sleep(2) # sleep 1s to wait for the server
        pid_second = get_server_pid_from_file()

        assert check_hdc_cmd("-l5 kill -r", "Kill server finish")
        time.sleep(2) # sleep 1s to wait for the server
        pid_third = get_server_pid_from_file()
        assert not check_hdc_cmd("start -r", "start server")
        time.sleep(1) # sleep 1s to wait for the server
        pid_forth = get_server_pid_from_file()
        assert pid_first != pid_second
        assert pid_second != pid_third
        assert pid_third != pid_forth
        assert check_hdc_cmd("checkserver", "Ver")
        time.sleep(2) # sleep 2s to wait for the server


    @pytest.mark.L0
    def test_server_checkserver(self):
        check_hdc_cmd('start')
        check_hdc_cmd('kill', "Kill server finish")
        server1 = check_hdc_cmd('checkserver', "Client version:Ver:")
        check_hdc_cmd('start')
        server2 = check_hdc_cmd('checkserver', "server version:Ver:")
        assert server1
        assert server2


    @pytest.mark.L0
    def test_server_lifecycle(self):
        check_hdc_cmd('start')
        kill = check_hdc_cmd('kill', "Kill server finish")
        check_hdc_cmd('start')
        pid1 = get_hdc_server_pid()

        check_hdc_cmd('start -r')
        pid2 = get_hdc_server_pid()

        check_hdc_cmd('start')
        pid3 = get_hdc_server_pid()

        del_server_pid_file()
        check_hdc_cmd('start -r')
        pid4 = get_hdc_server_pid()
        subprocess.run(f"taskkill /PID {pid4} /F", shell=True, capture_output=True, text=True)
        del_server_pid_file()
        check_hdc_cmd('start')
        assert kill
        assert pid1 != pid2
        assert pid2 == pid3
        assert pid3 == pid4


    @pytest.mark.L0
    def test_server_lifecycle2(self):
        check_hdc_cmd('start')
        check_hdc_cmd('kill', "Kill server finish")
        pid1 = get_hdc_server_pid()

        check_hdc_cmd('start')
        pid2 = get_hdc_server_pid()

        check_hdc_cmd('kill', "Kill server finish")
        pid3 = get_hdc_server_pid()

        check_hdc_cmd('start')
        pid4 = get_hdc_server_pid()

        del_server_pid_file()
        check_hdc_cmd('kill', "Kill server finish")
        pid5 = get_hdc_server_pid()

        check_hdc_cmd('kill -r', "Kill server finish")
        pid6 = get_hdc_server_pid()

        check_hdc_cmd('kill -r', "Kill server finish")
        pid7 = get_hdc_server_pid()
        del_server_pid_file()
        subprocess.run(f"taskkill /PID {pid7} /F", shell=True, capture_output=True, text=True)
        check_hdc_cmd('start')

        assert pid1 == None
        assert pid2 != ''
        assert pid3 != ''
        assert pid4 != ''
        assert pid5 == pid4
        assert pid6 != ''
        assert pid7 != ''
        assert pid6 == pid7
