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
import re
import subprocess
import time
import logging
import multiprocessing
import pytest
import threading

from utils import GP, check_hdc_cmd, check_shell, run_command_with_timeout, get_end_symbol, get_shell_result
from enum import Enum


class OsValue(Enum):
    UNKNOWN = 1
    HMOS = 2
    OTHER = 3


logger = logging.getLogger(__name__)
lock = threading.Lock()


class TestCommonSupport:
    subprocess_pid = 0
    os_value = OsValue.UNKNOWN

    @staticmethod
    def kill_process(pid):
        cmd = f"taskkill /F /PID {pid}"
        subprocess.Popen(cmd, shell=False)

    def new_process_run(self, cmd):
        process = subprocess.Popen(cmd, shell=True)
        self.subprocess_pid = process.pid
    def is_hmos(self):  # 此项依赖toybox和实际的OS
        with lock:
            if self.os_value != OsValue.UNKNOWN:
                return self.os_value == OsValue.HMOS
            if check_shell(f"shell uname -a", "HongMeng Kernel"):
                self.os_value = OsValue.HMOS
            else:
                self.os_value = OsValue.OTHER
        return self.os_value == OsValue.HMOS

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_target_key(self):
        device_key = get_shell_result(f"list targets").split("\r\n")[0]
        hdcd_pid = get_shell_result(f"-t {device_key} shell pgrep -x hdcd").split("\r\n")[0]
        assert hdcd_pid.isdigit()

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_ide_sdk_compatible(self):
        output_str, error_str = run_command_with_timeout(f"{GP.hdc_head} checkserver", 2)
        if output_str:  # Client version:Ver: 3.1.0e, server version:Ver: 3.1.0e
            output_str = output_str.replace(get_end_symbol(), "")
            logging.warning(output_str)
            versions = output_str.split(",")
            if len(versions) == 2:  # Client version:Ver: 3.1.0e     server version:Ver: 3.1.0e
                client_strs = versions[0].split(":")
                server_strs = versions[1].split(":")
                if len(client_strs) == 3 and len(server_strs) == 3:
                    logging.warning(client_strs[2])
                    logging.warning(server_strs[2])
                    assert client_strs[2] == server_strs[2]  # 3.1.0e
                    return
        assert False

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_sdk_support_bundle_file(self):
        client_version = get_shell_result(f"version")
        assert client_version >= "Ver: 3.1.0e"

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_daemon_support_bundle_file(self):
        daemon_version = get_shell_result(f"shell param get const.hdc.version")
        assert daemon_version >= "Ver: 3.1.0e"

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_software_version(self):
        software_version = get_shell_result(f"shell param get const.product.software.version")
        pattern = r'[A-Za-z0-9-]+ \d+.\d+.\d+.\d+([A-Za-z0-9]+)'
        match = re.search(pattern, software_version)
        assert match is not None

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_is_hongmeng_os(self):
        if not self.is_hmos():
            assert True
            return
        assert check_shell(f"shell uname -a", "HongMeng Kernel")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_support_ftp(self):
        if not self.is_hmos():
            assert True
            return
        assert not check_shell(f"shell ls -d /system/bin/bftpd", "No such file or directory")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_hiview_service_started(self):
        assert check_shell(f"shell hidumper -s 1201 -a '-p Faultlogger'", "HiviewService")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_bm_dump_support_options(self):
        assert not check_shell(f"shell bm dump -a", "error: unknown option")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_support_tsan(self):
        if not self.is_hmos():
            assert True
            return
        assert not check_shell(f"shell ls -d system/lib64/libclang_rt.tsan.so", "No such file or directory")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_support_faultlog_with_ms(self):
        output = get_shell_result(f"shell hidumper -s 1201 -a '-p Faultlogger -LogSuffixWithMs'")
        assert "HiviewService" in output
        assert "Unknown command" not in output

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_screenshot_with_png_format(self):
        assert check_shell(f"shell snapshot_display -f /data/local/tmp/example.png -t png", "success")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_hilog_not_support_options(self):
        assert check_shell(f"shell hilog -v xxx", "Invalid argument")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_support_screenrecorder(self):
        if not self.is_hmos():
            assert True
            return
        check_shell(f"shell aa start -b com.huawei.hmos.screenrecorder"
                    f" -a com.huawei.hmos.screenrecorder.ServiceExtAbility"
                    f" --ps 'CustomizedFileName' 'test.mp4'")
        time.sleep(2)
        assert check_shell(f"shell mediatool query test.xxx", "The displayName format is not correct!")
        assert not check_shell(f"shell mediatool query testxxx.mp4", "The displayName format is not correct!")
        result = get_shell_result(f"shell mediatool query test.mp4 -u")
        assert "test.mp4" in result and "file://media" in result

        check_shell(f"hdc shell aa start -b com.huawei.hmos.screenrecorder "
                    f"-a com.huawei.hmos.screenrecorder.ServiceExtAbility")

    def check_track_jpid(self):
        track_file = f"track_jpid_{time.time()}.log"
        cmd = f"hdc track-jpid -a > {track_file}"
        p = multiprocessing.Process(target=self.new_process_run, args=(cmd,))
        p.start()

        time.sleep(2)
        self.kill_process(p.pid)
        check_hdc_cmd("kill")
        p.join()
        p.close()

        # 读取track-jpid.log 解析 验证内容是否ok
        content_size = 0  # 所有表示长度的加起来
        first_line_size = 0  # 所有表示长度的内容长度之和

        size = 0
        with open(track_file, 'r') as file:
            for line in file.readlines():
                size = size + len(line)

                if " " in line:  # 数据行
                    print("data row")
                elif line == "\n":  # 最后一行
                    print("last row")
                else:
                    first_line_size = first_line_size + len(line)
                    line = line.replace(get_end_symbol(), "")
                    line = line.replace("\n", "")
                    content_size = content_size + int(line, 16)

        return size == first_line_size + content_size

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_check_track_jpid(self):
        assert self.check_track_jpid()
