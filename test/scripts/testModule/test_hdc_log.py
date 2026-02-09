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
import os
import re
import tempfile
import time
from utils import GP, get_cmd_block_output_and_error


class TestLog:
    @classmethod
    def setup_class(self):
        pass

    @classmethod
    def teardown_class(self):
        pass

    def search_log(self, value):
        exist = False
        temp_dir = tempfile.gettempdir()
        log_file = os.path.join(temp_dir, f"hdc.log")
        with open(log_file, f"r") as file:
            for line, text in enumerate(file, 1):
                if value in text:
                    exist = True
                    break
        return exist

    @pytest.mark.L0
    def test_log_name_over_128_chinese_character(self):
        count = 64
        chinese_str = f"中文"
        long_name = f"超长中文名称-"
        for i in range(count):
            long_name += chinese_str
        long_name += ".txt"
        temp_dir = tempfile.gettempdir()
        file_path = os.path.join(temp_dir, long_name)
        f = open(file_path, f"w+")
        f.close()
        assert os.path.exists(file_path)
        cmd_kill_r = f"{GP.hdc_exe} kill -r"
        result, error = get_cmd_block_output_and_error(cmd_kill_r)
        print(result, error)
        cmd_checkserver = f"{GP.hdc_exe} checkserver"
        result, error = get_cmd_block_output_and_error(cmd_checkserver)
        print(result, error)
        assert f"version" in result
        os.remove(file_path)

    @pytest.mark.L0
    def test_log_number_over_30_files(self):
        count = 30
        prefix = f"hdc-"
        suffix = f".log"
        timestr = time.strftime(f"%Y%m%d-%H%M%S")
        temp_dir = tempfile.gettempdir()
        for i in range(count):
            name = prefix + timestr + f"{i:03d}" + suffix
            log_file_path = os.path.join(temp_dir, name)
            f = open(log_file_path, f"w+")
            f.close()
        cmd_kill_r = f"{GP.hdc_exe} kill -r"
        result, error = get_cmd_block_output_and_error(cmd_kill_r)
        print(result, error)
        cmd_checkserver = f"{GP.hdc_exe} checkserver"
        result, error = get_cmd_block_output_and_error(cmd_checkserver)
        print(result, error)
        assert f"version" in result

    @pytest.mark.L0
    def test_log_for_one_device(self):
        devices = []
        cmd = f"{GP.hdc_exe} list targets"
        result, _ = get_cmd_block_output_and_error(cmd)
        print(result)
        result = result.split(f"\n")
        for item in result:
            if item != f"\n" and item != f"":
                devices.append(item)
        assert len(devices) == 1
        exist = False
        value = f"HdcServer [ sessionId"
        temp_dir = tempfile.gettempdir()
        log_file = os.path.join(temp_dir, f"hdc.log")
        with open(log_file, f"r") as file:
            for line, text in enumerate(file, 1):
                if value in text:
                    print(f"{line}: {text.strip()}")
                    exist = True
                    break
        assert exist

    @pytest.mark.L0
    def test_log_for_multi_tcp_disconnect(self):
        remote_port = 7777
        cmd_tmode = f"{GP.hdc_head} tmode port {remote_port}"
        result, _ = get_cmd_block_output_and_error(cmd_tmode)
        print(result)
        expect = f"Set device run mode successful"
        assert expect in result
        local_ip = f"127.0.0.1"
        port = 11111
        for i in range(4):
            local_port = port + i
            cmd_fport = f"{GP.hdc_head} fport tcp:{local_port} tcp:{remote_port}"
            result, _ = get_cmd_block_output_and_error(cmd_fport)
            expect = f"Forwardport result:OK"
            assert expect in result
            cmd_tconn = f"{GP.hdc_exe} tconn {local_ip}:{local_port}"
            result, _ = get_cmd_block_output_and_error(cmd_tconn)
            expect = f"Connect OK"
            assert expect in result
            cmd_shell_ls = f"{GP.hdc_exe} -t {local_ip}:{local_port} shell ls"
            result, _ = get_cmd_block_output_and_error(cmd_shell_ls)
            expect = f"bin"
            assert expect in result
            time.sleep(2)
            item = f"{local_ip}:{local_port}"
            connect_tcp = f"{item[:3]}******{item[-3:]}(L:{len(item)})"
            expect = f"connectKey:{connect_tcp} command flag:1001 command result:1 command take time"
            assert self.search_log(expect)
        for i in range(4):
            local_port = port + i
            cmd_fport_rm = f"{GP.hdc_head} fport rm tcp:{local_port} tcp:{remote_port}"
            result, _ = get_cmd_block_output_and_error(cmd_fport_rm)
            expect = f"Remove forward ruler success"
            assert expect in result
            time.sleep(2)
            item = f"{local_ip}:{local_port}"
            connect_tcp = f"{item[:3]}******{item[-3:]}(L:{len(item)})"
            expect = f"handshakeOK:1 connectKey:{connect_tcp} connType:1 ]"
            assert self.search_log(expect)

