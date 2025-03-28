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
import stat
import threading
import time
import logging
import multiprocessing
import pytest


from utils import GP, run_command_with_timeout, get_shell_result, \
    check_shell, check_version, get_local_path, rmdir, load_gp, get_hdcd_pss, get_hdcd_fd_count


SEP = "/"
MOUNT_POINT = "storage"
TEST_FILE_SIZE = 20  # 20KB
SEND_FILE_PROCESS_COUNT = 25
FD_THRESHOLD = 2
TEST_FILE_CASE_TABLE = [
    (False, False, True),
    (False, False, False),
    (False, True, True),
    (False, True, False),
    (True, False, True),
    (True, False, False),
    (True, True, True),
    (True, True, False),
]


logger = logging.getLogger(__name__)


def create_test_file(file_path, size, random=False):
    flags = os.O_CREAT | os.O_WRONLY | os.O_EXCL
    modes = stat.S_IWUSR | stat.S_IRUSR
    with os.fdopen(os.open(file_path, flags, modes), 'wb') as f:
        if random:
            f.write(os.urandom(size * 1024))  # 100KB
        else:
            f.seek(size * 1024 - 1)  # 移动到文件末尾
            f.write(b'\xff')  # 写入一个零字节


def create_binary_tree(depth, path='.', size=TEST_FILE_SIZE, random=False):
    if depth == 0:
        create_test_file(os.path.join(path, f'{size}KB.1.bin'), size, random=random)
        create_test_file(os.path.join(path, f'{size}KB.2.bin'), size, random=random)
    else:
        # 创建左右子目录
        left_path = os.path.join(path, '1')
        right_path = os.path.join(path, '2')
        os.makedirs(left_path, exist_ok=True)
        os.makedirs(right_path, exist_ok=True)

        # 递归创建下一层目录
        create_binary_tree(depth - 1, left_path, size)
        create_binary_tree(depth - 1, right_path, size)


class TestFileNoSpaceFdLeak:
    """
    直接填满磁盘空间进行文件传输，传输后查询fd泄露状态。
    """
    fd_count = 0
    pss = 0

    @staticmethod
    def send_file_to_storage(is_compress=False, is_directory=False, is_zero=False, is_mix=False):
        compress_command = "-z" if is_compress else ""
        single_file_name = "medium" if not is_zero else "word_100M.txt"
        single_dir_name = "tree_rand" if not is_zero else "tree_zero"
        local_path = get_local_path(single_dir_name) if is_directory else get_local_path(single_file_name)
        target_path = "it_nospace" if is_directory else "it_nospace.bin"
        if is_mix:
            local_path = get_local_path(".")
            target_path = "it_nospace_mix"
        re_send_time = 20
        for i in range(1, re_send_time + 1):
            logger.info("send %d times", i)
            output_str, error_str = run_command_with_timeout(f"{GP.hdc_head} "
                f"file send {compress_command} {local_path} {SEP}{MOUNT_POINT}/{target_path}_{i}", 25)
            if "Command timed out" in error_str:
                logger.warning("error_str: %s", error_str)
                return False
            if "space left on device" not in output_str:
                logger.warning(f"output_str: %s", output_str)
                return False
        return True

    def teardown_class(self):
        check_shell(f"shell rm -rf {SEP}{MOUNT_POINT}/it_*")
        rmdir(get_local_path("tree_rand"))
        rmdir(get_local_path("tree_zero"))
        pss_now = get_hdcd_pss()
        if self.pss == 0 or pss_now == 0:
            logger.error("get hdcd mem pss failed")
        if pss_now > (self.pss + 50):
            logger.warning("hdcd mem pss leak, original value %d, now value %d", self.pss, pss_now)

    def setup_class(self):
        depth = 10  # 目录深度
        if not os.path.exists(get_local_path("tree_rand")):
            create_binary_tree(depth, get_local_path("tree_rand"), random=True)
        if not os.path.exists(get_local_path("tree_zero")):
            create_binary_tree(depth, get_local_path("tree_zero"), random=False)
        check_shell(f"shell rm -rf {SEP}{MOUNT_POINT}/it_*")
        assert check_shell(f"shell dd if={SEP}dev/zero bs=500M count=24 of={SEP}storage/it_full.img",
                           "space left on device")
        time.sleep(1)
        self.fd_count = get_hdcd_fd_count()
        self.pss = get_hdcd_pss()
        if self.pss == 0:
            logger.error("get hdcd mem pss failed")


    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("is_compress, is_directory, is_zero", TEST_FILE_CASE_TABLE,
                             ids=[f"is_compress:{is_compress}, is_directory:{is_directory}, is_zero:{is_zero}"
                                  for is_compress, is_directory, is_zero in TEST_FILE_CASE_TABLE])
    def test_file_normal(self, is_compress, is_directory, is_zero):
        assert self.send_file_to_storage(is_compress=is_compress, is_directory=is_directory, is_zero=is_zero)
        pss_now = get_hdcd_pss()
        if self.pss == 0 or pss_now == 0:
            logger.error("get hdcd mem pss failed")
        if pss_now > (self.pss + 50):
            logger.warning("hdcd mem pss leak, original value %d, now value %d", self.pss, pss_now)

    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("is_compress", [True, False], ids=["is_compress:True", "is_compress:False"])
    def test_file_mix(self, is_compress):
        assert self.send_file_to_storage(is_compress=is_compress, is_mix=True)
        pss_now = get_hdcd_pss()
        if self.pss == 0 or pss_now == 0:
            logger.error("get hdcd mem pss failed")
        if pss_now > (self.pss + 50):
            logger.warning("hdcd mem pss leak, original value %d, now value %d", self.pss, pss_now)

    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_fd_leak_proc(self):
        assert not check_shell(f"shell ls -al {SEP}proc/`pidof hdcd`/fd", "it_nospace")

    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_fd_count(self):
        time.sleep(1)
        assert get_hdcd_fd_count() <= (self.fd_count + FD_THRESHOLD)


class TestFileReFullSpaceFdLeak:
    """
    磁盘空间接近满情况，进行文件传输，不断地删除并重传，传输后查询fd泄露状态。
    """
    stop_flag = threading.Event()
    fd_count = 0
    pss = 0

    def re_create_file(self, num=600):
        for i in range(1, num):
            check_shell(f"shell rm -rf {SEP}{MOUNT_POINT}/it_*;"
                        f" dd if={SEP}dev/zero bs=1M count=10240 of="
                        f"{SEP}storage/it_full.img")
            logger.info("re create file count:%d", i)
            if self.stop_flag.is_set():
                break

    def re_send_file(self, is_compress=False, is_directory=False, is_zero=False, is_mix=False):
        re_create_file_thread = threading.Thread(target=self.re_create_file)
        re_create_file_thread.start()
        result = True
        compress_command = "-z" if is_compress else ""
        single_file_name = "medium" if not is_zero else "word_100M.txt"
        single_dir_name = "tree_rand" if not is_zero else "tree_zero"
        local_path = get_local_path(single_dir_name) if is_directory else get_local_path(single_file_name)
        target_path = "it_nospace" if is_directory else "it_nospace.bin"
        if is_mix:
            local_path = get_local_path('.')
            target_path = "it_nospace_mix"
        re_send_time = 10
        for i in range(1, re_send_time + 1):
            output_str, error_str = run_command_with_timeout(f"{GP.hdc_head} "
                f"file send {compress_command} {local_path} {SEP}{MOUNT_POINT}/{target_path}_{i}", 25)
            logger.info("output:%s,error:%s", output_str, error_str)
            if "Command timed out" in error_str:
                logger.warning("Command timed out")
                result = False
                break
            if "Transfer Stop" not in output_str:
                logger.warning("Transfer Stop NOT FOUNT")
                result = False
                break
        self.stop_flag.set()
        re_create_file_thread.join()
        return result

    def setup_class(self):
        depth = 10  # 目录深度
        if not os.path.exists(get_local_path("tree_rand")):
            create_binary_tree(depth, get_local_path("tree_rand"), random=True)
        if not os.path.exists(get_local_path("tree_zero")):
            create_binary_tree(depth, get_local_path("tree_zero"), random=False)
        check_shell(f"shell rm -rf {SEP}{MOUNT_POINT}/it_*")
        check_shell(f"shell rm -rf {SEP}{MOUNT_POINT}/gen_*")
        storage_size = get_shell_result(
            f"shell \"df {SEP}{MOUNT_POINT} | grep {MOUNT_POINT}\"").split()[3]
        logger.info("storage size =%s", storage_size)
        assert int(storage_size) >= 10
        gen_size = int(storage_size) - 10
        logger.info("gen size = %d", gen_size)
        check_shell(f"shell dd if={SEP}dev/zero bs=1K count={gen_size} of="
                    f"{SEP}{MOUNT_POINT}/gen_{gen_size}.img")
        time.sleep(1)
        self.fd_count = get_hdcd_fd_count()
        self.pss = get_hdcd_pss()
        if self.pss == 0:
            logger.error("get hdcd mem pss failed")

    def teardown_class(self):
        check_shell(f"shell rm -rf {SEP}{MOUNT_POINT}/it_*")
        check_shell(f"shell rm -rf {SEP}{MOUNT_POINT}/gen_*")
        rmdir(get_local_path("tree_rand"))
        rmdir(get_local_path("tree_zero"))
        pss_now = get_hdcd_pss()
        if self.pss == 0 or pss_now == 0:
            logger.error("get hdcd mem pss failed")
        if pss_now > (self.pss + 50):
            logger.warning("hdcd mem pss leak, original value %d, now value %d", self.pss, pss_now)

    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("is_compress, is_directory, is_zero", TEST_FILE_CASE_TABLE,
                             ids=[f"is_compress:{is_compress}, is_directory:{is_directory}, is_zero:{is_zero}"
                                  for is_compress, is_directory, is_zero in TEST_FILE_CASE_TABLE])
    def test_file_normal(self, is_compress, is_directory, is_zero):
        assert self.re_send_file(is_compress=is_compress, is_directory=is_directory, is_zero=is_zero)

    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("is_compress", [True, False], ids=["is_compress:True", "is_compress:False"])
    def test_file_mix(self, is_compress):
        assert self.re_send_file(is_compress=is_compress, is_mix=True)

    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_compress_z_fd_proc(self):
        assert not check_shell(f"shell ls -al {SEP}proc/`pidof hdcd`/fd", "it_nospace")

    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_compress_z_fd_count(self):
        time.sleep(1)
        assert get_hdcd_fd_count() <= (self.fd_count + FD_THRESHOLD)


class TestFileNoSpaceFdFullCrash:
    fd_count = "0"
    pid = "0"
    plist = list()

    @staticmethod
    def teardown_class(self):
        for p in self.plist:
            p.join()

    @staticmethod
    def new_process_run(cmd):
        with open(os.devnull, 'w') as devnull:
            old_stdout = os.dup2(devnull.fileno(), 1)
            old_stderr = os.dup2(devnull.fileno(), 2)
            try:
                check_shell(f"{cmd}")
            finally:
                os.dup2(old_stdout, 1)
                os.dup2(old_stderr, 2)

    def setup_class(self):
        depth = 10  # 目录深度
        if not os.path.exists(get_local_path("tree")):
            create_binary_tree(depth, get_local_path("tree"), random=True)
        check_shell(f"shell rm -rf {SEP}{MOUNT_POINT}/it_*")
        check_shell(f"shell dd if={SEP}dev/zero bs=1K count=10 of={SEP}storage/smallfile")
        check_shell(f"shell dd if={SEP}dev/zero bs=1M of={SEP}storage/largefile")
        check_shell(f"shell df {SEP}storage")
        check_shell(f"shell rm -rf {SEP}storage/smallfile")
        time.sleep(1)
        check_shell(f"shell ls")
        self.pid = get_shell_result(f"shell pidof hdcd").split("\r")[0]
        self.fd_count = get_shell_result(f"shell ls {SEP}proc/{self.pid}/fd | wc -l")

    @pytest.mark.L2
    @check_version("Ver: 3.1.0e")
    def test_file_fd_full_no_crash(self):
        for _ in range(1, SEND_FILE_PROCESS_COUNT):
            cmd = f"file send {get_local_path('tree')} {SEP}storage/it_tree"
            p = multiprocessing.Process(target=self.new_process_run, args=(cmd,))
            p.start()
            self.plist.append(p)

        last_fd_count = 0
        loop_count = 0
        while True:
            self.fd_count = get_shell_result(f"shell ls {SEP}proc/{self.pid}/fd | wc -l")
            try:
                if int(self.fd_count) >= 32768:
                    break
                if int(self.fd_count) < last_fd_count:
                    run_command_with_timeout(f"{GP.hdc_head} kill", 3)
                    return
                last_fd_count = int(self.fd_count)
                loop_count += 1
                if loop_count % 5 == 0:
                    logger.warning("fd count is:%s", self.fd_count)
            except ValueError:
                logger.warning("ValueError, last count:%d", last_fd_count)
                break
        run_command_with_timeout(f"{GP.hdc_head} kill", 3)
        run_command_with_timeout(f"{GP.hdc_head} kill", 3)

        run_command_with_timeout(f"{GP.hdc_head} wait", 3)
        time.sleep(3)

        new_pid = get_shell_result(f"shell pidof hdcd").split("\r")[0]
        assert not self.pid == new_pid
