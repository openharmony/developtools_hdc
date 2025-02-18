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
import threading
import time
import pytest

from utils import GP, run_command_with_timeout, get_shell_result, \
    check_shell, check_version, get_local_path, load_gp \


sep = "/"
mount_point = "storage"
fd_count = 0


def create_test_file(file_path, size=200, random=False):
    with open(file_path, 'wb') as f:
        if random:
            f.write(os.urandom(size * 1024))  # 100KB
        else:
            f.seek(size * 1024 - 1)  # 移动到文件末尾
            f.write(b'\xff')  # 写入一个零字节


def create_binary_tree(depth, path='.', size=2000):
    if depth == 0:
        create_test_file(os.path.join(path, f'{size}KB.rand'), size, random=True)
        create_test_file(os.path.join(path, f'{size}KB.same'), size, random=False)
    else:
        # 创建左右子目录
        left_path = os.path.join(path, '1')
        right_path = os.path.join(path, '2')
        os.makedirs(left_path, exist_ok=True)
        os.makedirs(right_path, exist_ok=True)
        
        # 递归创建下一层目录
        create_binary_tree(depth - 1, left_path)
        create_binary_tree(depth - 1, right_path)


class TestFileNoSpace:
    def send_file_to_storage():
        if not check_shell(f"shell dd if={sep}dev/zero bs=500M count=24 of={sep}storage/it_full.img",
            "space left on device"):
            return False
        re_send_time = 100
        for i in range(1, re_send_time + 1):
            print(f"send {i} times")
            output_str, error_str = run_command_with_timeout(f"{GP.hdc_head} "
                f"file send -z {get_local_path('.')} {sep}{mount_point}/it_nospace_{i}", 25)
            if "Command timed out" in error_str:
                print(f"error_str: {error_str}")
                return False
            if "space left on device" not in output_str:
                print(f"output_str: {output_str}")
                return False
        return True

    @classmethod
    def setup_class(self):
        check_shell(f"shell rm -rf {sep}{mount_point}/it_*")
        check_shell("shell pkill hdcd")
        time.sleep(3)
        run_command_with_timeout(f"{GP.hdc_head} wait", 3) # reboot takes up to 30 seconds
        self.fd_count = get_shell_result(f"shell ls -al {sep}proc/`pidof hdcd`/fd | wc -l")

    @classmethod
    def teardown_class(self):
        check_shell(f"shell rm -rf {sep}{mount_point}/it_*")

    @pytest.mark.run(order=1)
    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_compress_z_fd_leak(self):
        assert self.send_file_to_storage()

    @pytest.mark.run(order=2)
    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_compress_z_fd_proc(self):
        assert not check_shell(f"shell ls -al {sep}proc/`pidof hdcd`/fd", "it_nospace")

    @pytest.mark.run(order=3)
    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_compress_z_fd_count(self):
        assert get_shell_result(f"shell ls -al {sep}proc/`pidof hdcd`/fd | wc -l") <= self.fd_count


class TestFileReFullSpace:
    stop_flag = threading.Event()

    def re_create_file(self, num=600):
        for i in range(1, num):
            check_shell(f"shell rm -rf {sep}{mount_point}/it_*;"
                        f" dd if={sep}dev/zero bs=1M count=10240 of="
                        f"{sep}storage/it_full.img")
            print(f"re create file count:{i}")
            if self.stop_flag.is_set():
                break

    def re_send_file(self):
        re_create_file_thread = threading.Thread(target=self.re_create_file)
        re_create_file_thread.start()
        result = True
        re_send_time = 100
        for i in range(1, re_send_time + 1):
            output_str, error_str = run_command_with_timeout(f"{GP.hdc_head} "
                f"file send -z {get_local_path('.')} {sep}{mount_point}/it_nospace_{i}", 25)
            print(f"output:{output_str},error:{error_str}")
            if "Command timed out" in error_str:
                print("Command timed out")
                result = False
                break
            if "space left on device" not in output_str:
                print("space left on device NOT FOUNT")
                result = False
                break
        self.stop_flag.set()
        re_create_file_thread.join()
        return result

    @classmethod
    def setup_class(self):
        if not os.path.exists(get_local_path("tree")):
            depth = 5  # 目录深度
            create_binary_tree(depth, get_local_path("tree"), size=2048)
        check_shell(f"shell rm -rf {sep}{mount_point}/it_*")
        check_shell(f"shell rm -rf {sep}{mount_point}/gen_*")
        storage_size = get_shell_result(
            f"shell \"df {sep}{mount_point} | grep {mount_point}\"").split()[3]
        print(storage_size)
        assert int(storage_size) >= 10
        gen_size = int(storage_size) - 10
        print(gen_size)
        check_shell(f"shell dd if={sep}dev/zero bs={gen_size}K count=1 of="
                    f"{sep}{mount_point}/gen_{gen_size}.img")
        check_shell("shell pkill hdcd")
        time.sleep(3)
        run_command_with_timeout(f"{GP.hdc_head} wait", 3)
        self.fd_count = get_shell_result(f"shell ls -al {sep}proc/`pidof hdcd`/fd | wc -l")

    @classmethod
    def teardown_class(self):
        check_shell(f"shell rm -rf {sep}{mount_point}/it_*")
        check_shell(f"shell rm -rf {sep}{mount_point}/gen_*")

    @pytest.mark.run(order=1)
    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_compress_z_processing(self):
        assert self.re_send_file()

    @pytest.mark.run(order=2)
    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_compress_z_fd_proc(self):
        assert not check_shell(f"shell ls -al {sep}proc/`pidof hdcd`/fd", "it_nospace")

    @pytest.mark.run(order=3)
    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_compress_z_fd_count(self):
        assert get_shell_result(f"shell ls -al {sep}proc/`pidof hdcd`/fd | wc -l") == self.fd_count