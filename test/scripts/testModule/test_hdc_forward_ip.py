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
import multiprocessing
import pytest

from utils import run_command_with_timeout, check_shell, load_gp


class TestForwardIpSetting:

    @staticmethod
    def new_process_run(cmd):
        # 重定向 stdout 和 stderr 到 /dev/null
        with open(os.devnull, 'w') as devnull:
            old_stdout = os.dup2(devnull.fileno(), 1)  # 重定向 stdout
            old_stderr = os.dup2(devnull.fileno(), 2)  # 重定向 stderr
            try:
                # 这里是子进程的代码，不会有任何输出到控制台
                check_shell(f'{cmd}')
            finally:
                # 恢复原始的 stdout 和 stderr
                os.dup2(old_stdout, 1)
                os.dup2(old_stderr, 2)

    def netstat(self, pattern):
        if os.name == 'nt':
            result = run_command_with_timeout(f"netstat -ano | findstr {pattern}", 5)
            return result
        else:
            result = run_command_with_timeout(f"netstat -anp | grep {pattern}", 5)
            return result

    @pytest.mark.L1
    @pytest.mark.repeat(1)
    def test_ip_format_valid(self):
        check_shell(f"kill")
        time.sleep(1)
        assert check_shell(f"-e 123 -m", f"-e content IP incorrect")
        assert check_shell(f"-e abd -m", f"-e content IP incorrect")
        assert check_shell(f"-e 测试 -m", f"-e content IP incorrect")
        assert check_shell(f"-e 12.22.22.22.1 -m", f"-e content IP incorrect")
        assert check_shell(f"-e 123.aba -m", f"-e content IP incorrect")
        assert check_shell(f"-e 323.21.21.2 -m", f"-e content IP incorrect")

    @pytest.mark.L1
    @pytest.mark.repeat(1)
    def test_ip_valid_1(self):
        check_shell(f"kill")
        time.sleep(1)

        p = multiprocessing.Process(target=self.new_process_run, args=("-e 0.0.0.0 -m",))
        p.start()
        time.sleep(1)

        result = self.netstat("127.0.0.1:8710")
        assert len(result) > 0

        assert check_shell(f"fport tcp:9988 tcp:8899", "Forwardport result:OK")
        result = self.netstat("0.0.0.0:9988")
        assert len(result) > 0

        assert check_shell(f"fport rm tcp:9988 tcp:8899", "success")

        check_shell(f"kill")
        time.sleep(1)

    @pytest.mark.L1
    @pytest.mark.repeat(1)
    def test_ip_valid_2(self):
        check_shell(f"kill")
        time.sleep(1)

        p = multiprocessing.Process(target=self.new_process_run, args=("-e 127.0.0.1 -m",))
        p.start()
        time.sleep(1)

        result = self.netstat("127.0.0.1:8710")
        assert len(result) > 0

        assert check_shell(f"fport tcp:9988 tcp:8899", "Forwardport result:OK")
        result = self.netstat("127.0.0.1:9988")
        assert len(result) > 0

        assert check_shell(f"fport rm tcp:9988 tcp:8899", "success")

        check_shell(f"kill")
        time.sleep(1)

    @pytest.mark.L1
    @pytest.mark.repeat(1)
    def test_ip_valid_3(self):
        check_shell(f"kill")
        time.sleep(1)

        p = multiprocessing.Process(target=self.new_process_run, args=("-m",))
        p.start()
        time.sleep(1)

        result = self.netstat("127.0.0.1:8710")
        assert len(result) > 0

        assert check_shell(f"fport tcp:9988 tcp:8899", "Forwardport result:OK")
        result = self.netstat("127.0.0.1:9988")
        assert len(result) > 0

        assert check_shell(f"fport rm tcp:9988 tcp:8899", "success")

        check_shell(f"kill")
        time.sleep(1)


    @pytest.mark.L1
    @pytest.mark.repeat(1)
    def test_ip_valid_3(self):
        check_shell(f"kill")
        time.sleep(1)

        p = multiprocessing.Process(target=self.new_process_run, args=("-e 10.176.223.250 -m",))
        p.start()
        time.sleep(1)

        result = self.netstat("127.0.0.1:8710")
        assert len(result) > 0
        assert check_shell(f"fport tcp:9988 tcp:8899", "failed")

        check_shell(f"kill")
        time.sleep(1)

    @pytest.mark.L1
    def test_ip_valid_4(self):
        check_shell(f"kill")
        time.sleep(1)

        p = multiprocessing.Process(target=self.new_process_run, args=("-e ::1 -m",))
        p.start()
        time.sleep(1)

        result = self.netstat("127.0.0.1:8710")
        assert len(result) > 0

        assert check_shell(f"fport tcp:9988 tcp:8899", "Forwardport result:OK")
        result = self.netstat("127.0.0.1:9988")
        assert len(result) > 0

        assert check_shell(f"fport rm tcp:9988 tcp:8899", "success")

        check_shell(f"kill")
        time.sleep(1)
