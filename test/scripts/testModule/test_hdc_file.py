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
import pytest
import subprocess
import sys

from utils import GP, check_hdc_cmd, check_hdc_targets, check_soft_local, check_soft_remote, \
    check_shell, rmdir, check_version, get_local_path, get_remote_path, make_multiprocess_file, \
    load_gp, get_md5sum_local, generate_random_string, get_shell_result

def get_hdcd_pid():
    result = subprocess.run('hdc shell pidof hdcd', shell=True, capture_output=True, text=True).stdout
    if result.strip():
        return result.split()[0]
    return None

def get_hdcd_fd_num():
    pid = get_hdcd_pid()
    if (pid == None):
        return None
    result = subprocess.run(f'ls -al /proc/{pid}/fd | wc -l', shell=True, capture_output=True, text=True).stdout
    if result.strip():
        return result.split()[0]
    return None


def clear_env():
    check_hdc_cmd("shell rm -rf data/local/tmp/it_*")
    check_hdc_cmd("shell mkdir data/local/tmp/it_send_dir")


def list_targets():
    assert check_hdc_targets()


# prehandle: generate file in device dir(/local/tmp/local)
def gen_file_in_tmp_dir(dir_name, file_name, file_size, unit, count, file_count):
    check_hdc_cmd(f"shell mkdir -p /data/local/tmp/{dir_name}")
    remote_path = dir_name + "/" + file_name
    if file_count == 1:
        check_hdc_cmd(f"shell dd if=/dev/urandom of=/data/local/tmp/{remote_path} bs={file_size}{unit} count={count}")
    else:
        for i in range(file_count):
            check_hdc_cmd(f"shell dd if=/dev/urandom of=/data/local/tmp/{remote_path}_{i} bs={file_size}{unit} count={count}")


# posthandle: delete file in device dir(/local/tmp/local)
def del_file_in_tmp_dir(dir_name, file_name_prefix, file_only=True):
    if file_only:
        remote_path = dir_name + "/" + file_name_prefix
        check_hdc_cmd(f"shell rm /data/local/tmp/{remote_path}*")
    else:
        check_hdc_cmd(f"shell rm -r /data/local/tmp/{dir_name}")

class TestFileCompress:
    compress_file_table = [
        ("medium", "it_medium_z"),
        ("word_100M.txt", "word_100M_compress.txt")
    ]

    @pytest.mark.L1
    @pytest.mark.repeat(1)
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("local_path, remote_path", compress_file_table)
    def test_file_compress(self, local_path, remote_path):
        clear_env()
        local_full_path = get_local_path(local_path)
        remote_full_path = get_remote_path(remote_path)
        local_recv_full_path = get_local_path(f'{local_path}_recv')
        try:
            assert check_hdc_cmd(f"file send -z {local_full_path} {remote_full_path}")
            assert check_hdc_cmd(f"file recv -z {remote_full_path} {local_recv_full_path}")
        finally:
            del_file_in_tmp_dir("", remote_path)
            if os.path.exists(local_recv_full_path):
                os.remove(local_recv_full_path)

class TestFileBase:
    base_file_table = [
        ("small", "it_small"),
        ("medium", "it_medium"),
        ("large", "it_large"),
        ("word_100M.txt", "word_100M")
    ]

    # file transfer rate baseline params
    file_transfer_count = 5
    file_transfer_avg_rate = 0.0

    @classmethod
    def setup_class(self):
        clear_env()

    def get_file_transfer_rate(self):
        file_send_avg_rate = 0.0
        file_recv_avg_rate = 0.0
        for i in range(TestFileBase.file_transfer_count):
            file_name = "large"
            send_remote_path = ""
            send_output = get_shell_result(f"file send {get_local_path(file_name)} {get_remote_path(send_remote_path)}")

            recv_local_file_name = f"{file_name}_recv"
            recv_output = get_shell_result(f"file recv {get_remote_path(file_name)} {get_local_path(recv_local_file_name)}")

            send_rate_str = send_output[send_output.rfind("rate:") + 5: send_output.rfind("kB/s")]
            recv_rate_str = recv_output[recv_output.rfind("rate:") + 5: recv_output.rfind("kB/s")]

            file_send_avg_rate += float(send_rate_str)
            file_recv_avg_rate += float(recv_rate_str)

        TestFileBase.file_transfer_avg_rate = (file_send_avg_rate + file_recv_avg_rate) / (TestFileBase.file_transfer_count * 2)
        del_file_in_tmp_dir("", file_name)

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    @pytest.mark.parametrize("local_path, remote_path", base_file_table)
    def test_file_normal(self, local_path, remote_path): 
        local_full_path = get_local_path(local_path)
        remote_full_path = get_remote_path(remote_path)
        local_recv_full_path = get_local_path(f'{local_path}_recv')
        try:
            assert check_hdc_cmd(f"file send {local_full_path} {remote_full_path}")
            md5_local = get_md5sum_local(local_full_path)
            assert check_hdc_cmd(f"file recv {remote_full_path} {local_recv_full_path}")
            md5_local_recv = get_md5sum_local(local_recv_full_path)
        finally:
            del_file_in_tmp_dir("", remote_path)
            if os.path.exists(local_recv_full_path):
                os.remove(local_recv_full_path)
        assert md5_local == md5_local_recv

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_file_empty(self):
        local_full_path = get_local_path("empty")
        remote_full_path = get_remote_path("it_empty")
        local_recv_full_path = get_local_path(f'empty_recv')
        try:
            assert check_hdc_cmd(f"file send {local_full_path} {remote_full_path}")
            assert check_hdc_cmd(f"file recv {remote_full_path} {local_recv_full_path}")
        finally:
            del_file_in_tmp_dir("", "it_empty")
            if os.path.exists(local_recv_full_path):
                os.remove(local_recv_full_path)

    @pytest.mark.L0
    def test_chinese_file_name_md5(self):
        filename = "中文"
        gen_file_in_tmp_dir("", filename, 100, "M", 1, 1)

        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(filename)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(filename)} {get_local_path(filename)}")
        md5_local = get_md5sum_local(get_local_path(filename))
        del_file_in_tmp_dir("", filename)
        if os.path.exists(get_local_path(filename)):
            os.remove(get_local_path(filename))
        assert md5_remote == md5_local

    @pytest.mark.L0
    def test_chinese_file_name_md5_2(self):
        filename = "中文chinese"
        gen_file_in_tmp_dir("", filename, 100, "M", 1, 1)

        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(filename)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(filename)} {get_local_path(filename)}")
        md5_local = get_md5sum_local(get_local_path(filename))
        del_file_in_tmp_dir("", filename)
        if os.path.exists(get_local_path(filename)):
            os.remove(get_local_path(filename))
        assert md5_remote == md5_local
    
    @pytest.mark.L0
    def test_chinese_file_name_md5_with_chinese_dir(self):
        filename = "中文"
        dirname = "中文目录"
        gen_file_in_tmp_dir(dirname, filename, 100, "M", 1, 1)

        remote_path = dirname + "/" + filename
        local_path = ""
        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(remote_path)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(dirname)} {get_local_path(local_path)}")
        md5_local = get_md5sum_local(get_local_path(os.path.join(dirname, filename)))
        del_file_in_tmp_dir(dirname, "", False)
        if os.path.exists(get_local_path(dirname)):
            rmdir(get_local_path(dirname))
        assert md5_remote == md5_local

    @pytest.mark.L0
    def test_chinese_file_name_md5_with_chinese_dir_2(self):
        filename = "中文chinese"
        dirname = "中文目录chinese"
        gen_file_in_tmp_dir(dirname, filename, 100, "M", 1, 1)

        remote_path = dirname + "/" + filename
        local_path = ""
        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(remote_path)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(dirname)} {get_local_path(local_path)}")
        md5_local = get_md5sum_local(get_local_path(os.path.join(dirname, filename)))
        del_file_in_tmp_dir(dirname, "", False)
        if os.path.exists(get_local_path(dirname)):
            rmdir(get_local_path(dirname))
        assert md5_remote == md5_local

    @pytest.mark.L0
    def test_file_md5_with_long_dir_file(self):
        file_name_10 = generate_random_string(10)
        dir_name_10 = generate_random_string(10)
        gen_file_in_tmp_dir(dir_name_10, file_name_10, 100, "M", 1, 1)
        
        remote_path = dir_name_10 + "/" + file_name_10
        local_path = ""
        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(remote_path)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(dir_name_10)} {get_local_path(local_path)}")
        md5_local = get_md5sum_local(get_local_path(os.path.join(dir_name_10, file_name_10)))
        del_file_in_tmp_dir(dir_name_10, "", False)
        if os.path.exists(get_local_path(dir_name_10)):
            rmdir(get_local_path(dir_name_10))
        assert md5_remote == md5_local

    @pytest.mark.L0
    def test_file_md5_with_long_dir_file_1(self):
        file_name_30 = generate_random_string(30)
        dir_name_30 = generate_random_string(30)
        gen_file_in_tmp_dir(dir_name_30, file_name_30, 100, "M", 1, 1)
        
        remote_path = dir_name_30 + "/" + file_name_30
        local_path = ""
        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(remote_path)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(dir_name_30)} {get_local_path(local_path)}")
        md5_local = get_md5sum_local(get_local_path(os.path.join(dir_name_30, file_name_30)))
        del_file_in_tmp_dir(dir_name_30, "", False)
        if os.path.exists(get_local_path(dir_name_30)):
            rmdir(get_local_path(dir_name_30))
        assert md5_remote == md5_local

    @pytest.mark.L0
    def test_file_md5_with_long_dir_file_2(self):
        file_name_64 = generate_random_string(64)
        dir_name_64 = generate_random_string(64)
        gen_file_in_tmp_dir(dir_name_64, file_name_64, 100, "M", 1, 1)
        
        remote_path = dir_name_64 + "/" + file_name_64
        local_path = ""
        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(remote_path)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(dir_name_64)} {get_local_path(local_path)}")
        md5_local = get_md5sum_local(get_local_path(os.path.join(dir_name_64, file_name_64)))
        del_file_in_tmp_dir(dir_name_64, "", False)
        if os.path.exists(get_local_path(dir_name_64)):
            rmdir(get_local_path(dir_name_64))
        assert md5_remote == md5_local

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_file_md5_with_option_cwd(self):
        file_name = "test_file"
        gen_file_in_tmp_dir("", file_name, 1, "M", 1, 1)
        try:
            md5_remote = get_shell_result(f"shell md5sum {get_remote_path(file_name)}").split()[0]
            assert check_hdc_cmd(f"file recv {get_remote_path(file_name)} {get_local_path(file_name)}")
            md5_local = get_md5sum_local(get_local_path(file_name))
            assert md5_remote == md5_local
            assert check_hdc_cmd(f"file send -cwd {get_local_path('')} {file_name} {get_remote_path(file_name)}")
        finally:
            del_file_in_tmp_dir("", file_name)
            if os.path.exists(get_local_path(file_name)):
                os.remove(get_local_path(file_name))

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_1G_file_md5(self):
        filename = "test_1G_file"
        gen_file_in_tmp_dir("", filename, 1024, "M", 1, 1)

        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(filename)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(filename)} {get_local_path(filename)}")
        md5_local = get_md5sum_local(get_local_path(filename))
        del_file_in_tmp_dir("", filename)
        if os.path.exists(get_local_path(filename)):
            os.remove(get_local_path(filename))
        assert md5_remote == md5_local

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_2G_file_md5(self):
        filename = "test_2G_file"
        gen_file_in_tmp_dir("", filename, 1024, "M", 2, 1)

        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(filename)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(filename)} {get_local_path(filename)}")
        md5_local = get_md5sum_local(get_local_path(filename))
        del_file_in_tmp_dir("", filename)
        if os.path.exists(get_local_path(filename)):
            os.remove(get_local_path(filename))
        assert md5_remote == md5_local


    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_4G_file_md5(self):
        filename = "test_4G_file"
        gen_file_in_tmp_dir("", filename, 1024, "M", 4, 1)

        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(filename)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(filename)} {get_local_path(filename)}")
        md5_local = get_md5sum_local(get_local_path(filename))
        del_file_in_tmp_dir("", filename)
        if os.path.exists(get_local_path(filename)):
            os.remove(get_local_path(filename))
        assert md5_remote == md5_local
    
    @pytest.mark.L0
    def test_6G_file_md5(self):
        filename = "test_6G_file"
        gen_file_in_tmp_dir("", filename, 1024, "M", 6, 1)

        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(filename)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(filename)} {get_local_path(filename)}")
        md5_local = get_md5sum_local(get_local_path(filename))
        del_file_in_tmp_dir("", filename)
        if os.path.exists(get_local_path(filename)):
            os.remove(get_local_path(filename))
        assert md5_remote == md5_local

    @pytest.mark.L0
    def test_10G_file_md5(self):
        filename = "test_10G_file"
        gen_file_in_tmp_dir("", filename, 1024, "M", 10, 1)

        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(filename)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(filename)} {get_local_path(filename)}")
        md5_local = get_md5sum_local(get_local_path(filename))
        del_file_in_tmp_dir("", filename)
        if os.path.exists(get_local_path(filename)):
            os.remove(get_local_path(filename))
        assert md5_remote == md5_local

    @pytest.mark.L0
    def test_16G_file_md5(self):
        filename = "test_16G_file"
        gen_file_in_tmp_dir("", filename, 1024, "M", 16, 1)

        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(filename)}").split()[0]
        check_hdc_cmd(f"file recv {get_remote_path(filename)} {get_local_path(filename)}")
        md5_local = get_md5sum_local(get_local_path(filename))
        del_file_in_tmp_dir("", filename)
        if os.path.exists(get_local_path(filename)):
            os.remove(get_local_path(filename))
        assert md5_remote == md5_local

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_file_md5_with_10_depth_dir(self):
        depth = 10
        dir_name, dir_name_tmp = "0", "0"
        for i in range(1, depth):
            dir_name = dir_name + "/" + str(i)
            if sys.platform == 'win32':
                dir_name_tmp = dir_name_tmp + "\\" + str(i)
            else:
                dir_name_tmp = dir_name_tmp + "/" + str(i)
        filename = "test_10G_file"
        gen_file_in_tmp_dir(dir_name, filename, 1, "M", 10, 1)

        remote_full_path = dir_name + "/" + filename
        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(remote_full_path)}").split()[0]
        remote_path = "0"
        local_path = ""
        check_hdc_cmd(f"file recv {get_remote_path(remote_path)} {get_local_path(local_path)}")
        file_full_path = os.path.join(dir_name_tmp, filename)
        md5_local = get_md5sum_local(get_local_path(file_full_path))
        del_file_in_tmp_dir(dir_name, filename)
        if os.path.exists(get_local_path("0")):
            rmdir(get_local_path("0"))
        assert md5_remote == md5_local

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_file_md5_with_20_depth_dir(self):
        depth = 20
        dir_name, dir_name_tmp = "0", "0"
        for i in range(1, depth):
            dir_name = dir_name + "/" + str(i)
            if sys.platform == 'win32':
                dir_name_tmp = dir_name_tmp + "\\" + str(i)
            else:
                dir_name_tmp = dir_name_tmp + "/" + str(i)
        filename = "test_10G_file"
        gen_file_in_tmp_dir(dir_name, filename, 1, "M", 10, 1)

        remote_full_path = dir_name + "/" + filename
        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(remote_full_path)}").split()[0]
        remote_path = "0"
        local_path = ""
        check_hdc_cmd(f"file recv {get_remote_path(remote_path)} {get_local_path(local_path)}")
        file_full_path = os.path.join(dir_name_tmp, filename)
        md5_local = get_md5sum_local(get_local_path(file_full_path))
        del_file_in_tmp_dir(dir_name, filename)
        if os.path.exists(get_local_path("0")):
            rmdir(get_local_path("0"))
        assert md5_remote == md5_local

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_file_md5_with_32_depth_dir(self):
        depth = 32
        dir_name, dir_name_tmp = "0", "0"
        for i in range(1, depth):
            dir_name = dir_name + "/" + str(i)
            if sys.platform == 'win32':
                dir_name_tmp = dir_name_tmp + "\\" + str(i)
            else:
                dir_name_tmp = dir_name_tmp + "/" + str(i)
        filename = "test_10G_file"
        gen_file_in_tmp_dir(dir_name, filename, 1, "M", 10, 1)

        remote_full_path = dir_name + "/" + filename
        md5_remote = get_shell_result(f"shell md5sum {get_remote_path(remote_full_path)}").split()[0]
        remote_path = "0"
        local_path = ""
        check_hdc_cmd(f"file recv {get_remote_path(remote_path)} {get_local_path(local_path)}")
        file_full_path = os.path.join(dir_name_tmp, filename)
        md5_local = get_md5sum_local(get_local_path(file_full_path))
        del_file_in_tmp_dir(dir_name, filename)
        if os.path.exists(get_local_path("0")):
            rmdir(get_local_path("0"))
        assert md5_remote == md5_local
    
    @pytest.mark.L0
    def test_file_rate_60M(self):
        # The value less than 0.001 indicates that file_transfer_avg_rate is a value that has not been initialized yet
        if TestFileBase.file_transfer_avg_rate < 0.001:
            self.get_file_transfer_rate()
        rate_60M = 1.0 * 60 * 1024
        assert TestFileBase.file_transfer_avg_rate >= rate_60M

    @pytest.mark.L0
    def test_file_rate_80M(self):
        # The value less than 0.001 indicates that file_transfer_avg_rate is a value that has not been initialized yet
        if TestFileBase.file_transfer_avg_rate < 0.001:
            self.get_file_transfer_rate()
        rate_80M = 1.0 * 80 * 1024
        assert TestFileBase.file_transfer_avg_rate >= rate_80M

    @pytest.mark.L0
    def test_file_rate_120M(self):
        # The value less than 0.001 indicates that file_transfer_avg_rate is a value that has not been initialized yet
        if TestFileBase.file_transfer_avg_rate < 0.001:
            self.get_file_transfer_rate()
        rate_120M = 1.0 * 120 * 1024
        assert TestFileBase.file_transfer_avg_rate >= rate_120M


class TestDirBase:
    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_empty_dir(self):
        clear_env()
        assert check_shell(f"file send {get_local_path('empty_dir')} {get_remote_path('it_empty_dir')}",
            "the source folder is empty")
        assert check_hdc_cmd("shell mkdir data/local/tmp/it_empty_dir_recv")
        assert check_shell(f"file recv {get_remote_path('it_empty_dir_recv')} {get_local_path('empty_dir_recv')}",
            "the source folder is empty")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_long_path(self):
        clear_env()
        assert check_hdc_cmd(f"file send {get_local_path('deep_test_dir')} {get_remote_path('it_send_dir')}",
                            is_single_dir=False)
        assert check_hdc_cmd(f"file recv {get_remote_path('it_send_dir/deep_test_dir')} "
                             f"{get_local_path('recv_test_dir')}",
                            is_single_dir=False)

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_recv_dir(self):
        if os.path.exists(get_local_path('it_problem_dir')):
            rmdir(get_local_path('it_problem_dir'))
        assert check_hdc_cmd(f"shell rm -rf {get_remote_path('it_problem_dir')}")
        assert check_hdc_cmd(f"shell rm -rf {get_remote_path('problem_dir')}")
        assert make_multiprocess_file(get_local_path('problem_dir'), get_remote_path(''), 'send', 1, "dir")
        assert check_hdc_cmd(f"shell mv {get_remote_path('problem_dir')} {get_remote_path('it_problem_dir')}")
        assert make_multiprocess_file(get_local_path(''), get_remote_path('it_problem_dir'), 'recv', 1, "dir")


class TestDirMix:
    muti_num = 5 # the count of multiprocess file
    file_table = ['empty', 'medium', 'small']

    @classmethod
    def setup_class(self):
        pass

    @classmethod
    def teardown_class(self):
        for test_item in self.file_table:
            for i in range(0, self.muti_num - 1):
                rmdir(get_local_path(f"{test_item}_recv_{i}"))
            
    @pytest.mark.L1
    @pytest.mark.repeat(2)
    @pytest.mark.parametrize("test_item", file_table)
    def test_mix_file(self, test_item):
        assert make_multiprocess_file(get_local_path(f'{test_item}'), get_remote_path(f'it_{test_item}'),
                                      'send', self.muti_num, "file")
        assert make_multiprocess_file(get_local_path(f'{test_item}_recv'), get_remote_path(f'it_{test_item}'),
                                      'recv', self.muti_num, "file")


class TestFileNoExist:
    remote_unexist_path = f"{get_remote_path('it_no_exist/deep_test_dir/')}"

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    @pytest.mark.parametrize("test_item", ['empty', 'medium', 'small', 'problem_dir'])
    def test_no_exist_path(self, test_item):
        check_hdc_cmd(f"shell rm -rf {get_remote_path('it_no_exist*')}")
        local_unexist_path = get_local_path(f'{(os.path.join("recv_no_exist", "deep_test_dir", f"recv_{test_item}"))}')
        if os.path.exists(get_local_path('recv_no_exist')):
            rmdir(get_local_path('recv_no_exist'))
        if test_item in ['empty', 'medium', 'small']:
            assert check_shell(f"file send "
                               f"{get_local_path(f'{test_item}')} {self.remote_unexist_path}/it_{test_item}",
                               "no such file or directory")
            assert check_shell(f"file recv {self.remote_unexist_path}/it_{test_item} "
                               f"{local_unexist_path}",
                               "no such file or directory")
        else:
            assert check_hdc_cmd(f"file send "
                                f"{get_local_path(f'{test_item}')} {self.remote_unexist_path}/it_{test_item}")
            assert check_hdc_cmd(f"file recv {self.remote_unexist_path}/it_{test_item} "
                                f"{local_unexist_path}")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    @pytest.mark.parametrize("test_item", ['empty', 'medium', 'small'])
    def test_no_exist_path_with_seperate(self, test_item):
        check_hdc_cmd(f"shell rm -rf {get_remote_path('it_no_exist*')}")
        local_unexist_path = get_local_path(f'{(os.path.join("recv_no_exist", "deep_test_dir"))}')
        if os.path.exists(get_local_path('recv_no_exist')):
            rmdir(get_local_path('recv_no_exist'))
        assert check_shell(f"file send "
                           f"{get_local_path(f'{test_item}')} {self.remote_unexist_path}/",
                           "no such file or directory")
        assert check_shell(f"file recv {self.remote_unexist_path}/{test_item} "
                           f"{local_unexist_path}{os.sep}",
                           "no such file or directory")


class TestFileNoExistBundelPath:
    data_storage_el2_path = "data/storage/el2/base"
    remote_unexist_path = f"{data_storage_el2_path}/it_no_exist/deep_test_dir/"

    @classmethod
    def setup_class(self):
        check_shell(f"shell mkdir -p mnt/debug/100/debug_hap/{GP.debug_app}/{self.data_storage_el2_path}")

    @pytest.mark.L0
    @pytest.mark.repeat(3)
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("test_item", ['empty', 'medium', 'small', 'problem_dir'])
    def test_no_exist_bundle_path(self, test_item):
        check_hdc_cmd(f"shell -b {GP.debug_app} rm -rf {self.data_storage_el2_path}/it_no_exist/")
        local_unexist_path = get_local_path(
            f'{(os.path.join("recv_no_exist", "deep_test_dir", f"recv_{test_item}"))}')
        if os.path.exists(get_local_path('recv_no_exist')):
            rmdir(get_local_path('recv_no_exist'))

        assert check_hdc_cmd(f"shell -b {GP.debug_app} ls {self.data_storage_el2_path}/it_no_exist",
                            "No such file or directory")
        assert not os.path.exists(get_local_path('recv_no_exist'))
        if test_item in ['empty', 'medium', 'small']:
            assert check_shell(f"file send -b {GP.debug_app} "
                               f"{get_local_path(f'{test_item}')} {self.remote_unexist_path}/it_{test_item}",
                               "no such file or directory")
            assert check_shell(f"file recv  -b {GP.debug_app} {self.remote_unexist_path}/it_{test_item} "
                               f"{local_unexist_path}",
                               "no such file or directory")
        else:
            assert check_hdc_cmd(f"file send -b {GP.debug_app} "
                                f"{get_local_path(f'{test_item}')} {self.remote_unexist_path}/it_{test_item}")
            assert check_hdc_cmd(f"file recv  -b {GP.debug_app} {self.remote_unexist_path}/it_{test_item} "
                                f"{local_unexist_path}")


class TestFileExtend:
    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_node_file(self):
        assert check_hdc_cmd(f"file recv {get_remote_path('../../../sys/power/state')} {get_local_path('state')}")
        assert check_hdc_cmd(f"file recv {get_remote_path('../../../sys/firmware/fdt')} {get_local_path('fdt')}")

    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_running_file(self):
        assert check_hdc_cmd(f"file recv system/bin/hdcd {get_local_path('running_recv')}")

    @pytest.mark.L2
    @pytest.mark.repeat(2)
    def test_soft_link(self):
        if not os.path.exists(get_local_path('soft_small')):
            pytest.skip("创建软链接失败，请使用管理员权限执行[python prepare.py -s]重新配置资源。")
        assert check_soft_local(get_local_path('small'), get_local_path('soft_small'), 
                                get_remote_path('it_small_soft'))
        assert check_soft_remote('it_small_soft', get_remote_path('it_soft_small'),
                                get_local_path('recv_soft_small'))

    @pytest.mark.L2
    @pytest.mark.repeat(2)
    def test_soft_dir(self):
        if not os.path.exists(get_local_path('soft_dir')):
            pytest.skip("创建软链接失败，请使用管理员权限执行[python prepare.py -s]重新配置资源。")
        assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('recv_small')}")
        assert check_hdc_cmd(f"file recv {get_remote_path('recv_small')} "
                               f"{os.path.join(get_local_path('soft_dir'), 'd2', 'it_recv_small')}")
        rmdir(os.path.join(get_local_path('soft_dir'), 'd2', 'it_recv_small'))


class TestFileError:
    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_file_error(self):
        assert check_hdc_cmd("target mount")
        assert check_shell(
            f"file recv",
            "[Fail]There is no local and remote path"
        )
        assert check_shell(
            f"file send",
            "[Fail]There is no local and remote path"
        )
        assert check_hdc_cmd(f"shell rm -rf {get_remote_path('../../../large')}")
        assert check_hdc_cmd(f"shell param set persist.hdc.control.file false")
        assert check_shell(
            f"file send {get_local_path('small')} {get_remote_path('it_small_false')}",
            "debugging is not allowed"
        )
        assert check_hdc_cmd(f"shell param set persist.hdc.control.file true")
        assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('it_small_true')}")


class TestFileBundleOptionNormal:
    data_storage_el2_path = "data/storage/el2/base"

    @classmethod
    def setup_class(self):
        check_shell(f"shell rm -rf mnt/debug/100/debug_hap/{GP.debug_app}/{self.data_storage_el2_path}/it*")
        check_shell(f"shell mkdir -p mnt/debug/100/debug_hap/{GP.debug_app}/{self.data_storage_el2_path}")

    @classmethod
    def teardown_class(self):
        pass

    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("test_item", ['empty', 'medium', 'small', 'problem_dir'])
    def test_file_option_bundle_normal(self, test_item):
        if test_item == 'problem_dir' and os.path.exists(get_local_path(f'recv_bundle_{test_item}')):
            rmdir(get_local_path(f'recv_bundle_{test_item}'))
        assert check_hdc_cmd(f"file send -b {GP.debug_app} "
                            f"{get_local_path(f'{test_item}')} {self.data_storage_el2_path}/it_{test_item}")
        assert check_hdc_cmd(f"file recv -b {GP.debug_app} {self.data_storage_el2_path}/it_{test_item} "
                            f"{get_local_path(f'recv_bundle_{test_item}')}")


class TestFileBundleOptionError:
    data_storage_el2_path = "data/storage/el2/base" 
    error_table = [
        # 测试空文件夹发送和接收
        ("Sending and receiving an empty folder",
         f"file send -b {GP.debug_app} {get_local_path('empty_dir')} {get_remote_path('it_empty_dir')}",
         "the source folder is empty"),

        # 测试缺少本地和远程路径
        ("Missing local and remote paths for send", f"file send -b {GP.debug_app}", "There is no local and remote path"),
        ("Missing local and remote paths for recv", f"file recv -b {GP.debug_app}", "There is no local and remote path"),

        # 测试错误优先级
        ("Error priority for send", f"file send -b ./{GP.debug_app}", "There is no local and remote path"),
        ("Error priority for recv", f"file recv -b ./{GP.debug_app}", "There is no local and remote path"),

        # 测试缺少 bundle 参数
        ("Missing bundle parameter for recv", f"file recv -b", "[E005003]"),
        ("Missing bundle parameter for send", f"file send -b", "[E005003]"),

        # 测试本地和远程路径错误
        ("Incorrect local and remote paths for send",
         f"file send -b ./{GP.debug_app} {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),
        ("Incorrect local and remote paths for recv",
         f"file recv -b ./{GP.debug_app} {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),

        # 测试无效的 bundle 参数
        ("invalid bundle parameter for send",
         f"file send -b ./{GP.debug_app} {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),
        ("invalid bundle parameter with valid name for send",
         f"file send -b com.AAAA {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),
        ("invalid bundle parameter with number for send",
         f"file send -b 1 {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),
        ("Missing remote path for send",
         f"file send -b {get_local_path('small')} {get_remote_path('it_small')}", "There is no remote path"),

        # 测试远程路径错误
        ("Incorrect remote path for recv",
         f"file recv -b ./{GP.debug_app} {get_remote_path('it_small')} {get_local_path('small_recv')}", "[E005101]"),

        # 测试路径逃逸
        ("Path escaping for send",
         f"file send -b {GP.debug_app} {get_local_path('small')} ../../../it_small", "[E005102]"),
        ("Path escaping for recv",
         f"file recv -b {GP.debug_app} ../../../it_small {get_local_path('small_recv')}", "[E005102]"),
    ]
    outside_error = "[E005102]"
    outside_table = [
        # bundle根目录 逃逸
        ("Escape to parent directory", "..", True),
        ("Escape two levels up", "../..", True),
        ("Escape three levels up", "../../..", True),
        ("Escape four levels up", "../../../..", True),
        ("Escape to parent directory with current directory", "../.", True),
        ("Escape to parent directory from current directory", "./..", True),
        ("Escape to parent directory with slash", "../", True),
        (f"Escape to extra app directory", f"../{GP.debug_app}_extra/{data_storage_el2_path}/", True),
        (f"Escape to non-existent app directory", f"../{GP.debug_app}_notexsit/{data_storage_el2_path}/", True),
        (f"Escape to non-existent app directory", f"../{GP.debug_app}_notexsit/", True),
        (f"Escape far beyond root", "./../../../../../../../../../../aa", True),

        # bundle根目录 未逃逸
        ("Stays in the current directory", ".", False),
        ("Stays in the current directory with slash", "./", False),
        ("Stays in the current directory with current directory", "./.", False),
        ("Stays in the current directory with ellipsis", "...", False),
        (f"Stays at root and return bundle",
            f"../../../../../../../../../../../mnt/debug/100/debug_hap/{GP.debug_app}/", False),
    ]

    @classmethod
    def setup_class(self):
        check_shell(f"shell rm -rf mnt/debug/100/debug_hap/{GP.debug_app}/{self.data_storage_el2_path}/it*")
        check_shell(f"shell mkdir -p mnt/debug/100/debug_hap/{GP.debug_app}/{self.data_storage_el2_path}")
        check_shell(f"shell rm -rf mnt/debug/100/debug_hap/{GP.debug_app}_extra/{self.data_storage_el2_path}/*")
        check_shell(f"shell mkdir -p mnt/debug/100/debug_hap/{GP.debug_app}_extra/{self.data_storage_el2_path}/")

    @classmethod
    def teardown_class(self):
        pass

    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    @pytest.mark.parametrize("test_name, command, expected", error_table,
                             ids=[name for name, _, _ in error_table])
    def test_file_option_bundle_error_(self, test_name, command, expected):
        assert check_shell(command, expected)

    @pytest.mark.parametrize("test_name, remote_path, is_outside", outside_table,
                             ids=[name for name, _, _ in outside_table])
    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    def test_file_option_bundle_check_outside(self, test_name, remote_path, is_outside):
        if is_outside:
            assert check_shell(f"file send -b {GP.debug_app} {get_local_path('small')} {remote_path}",
                               self.outside_error)
        else:
            assert not check_shell(f"file send -b {GP.debug_app} {get_local_path('small')} {remote_path}",
                                   self.outside_error)


class TestFullDisk:
    @pytest.mark.L0
    def test_full_disk(self):
        subprocess.run('hdc shell dd if=/dev/zero of=/storage/smallfile bs=1M count=10', shell=True, capture_output=True, text=True)
        subprocess.run('hdc shell dd if=/dev/zero of=/storage/largefile bs=1M', shell=True, capture_output=True, text=True)

        check_hdc_cmd('shell df -h')
        check_hdc_cmd('shell rm -rf /storage/smallfile')

        pid1 = get_hdcd_pid()
        fds1 = get_hdcd_fd_num()
        path_large = get_local_path('large')
        send = check_hdc_cmd(f'file send {path_large} /storage/large_full')
        pid2 = get_hdcd_pid()
        fds2 = get_hdcd_fd_num()

        check_hdc_cmd('shell rm -rf /storage/smallfile')
        check_hdc_cmd('shell rm -rf /storage/largefile')
        check_hdc_cmd('shell rm -rf /storage/large')

        assert pid1 != None
        assert pid1 == pid2
        assert fds1 == fds2
        assert send == False


    def test_full_disk2(self):
        subprocess.run('hdc shell dd if=/dev/zero of=/storage/smallfile bs=1M count=10', shell=True, capture_output=True, text=True)
        subprocess.run('hdc shell dd if=/dev/zero of=/storage/largefile bs=1M', shell=True, capture_output=True, text=True)

        check_hdc_cmd('shell df -h')
        check_hdc_cmd('shell rm -rf /storage/smallfile')

        pid1 = get_hdcd_pid()
        fds1 = get_hdcd_fd_num()
        path_local = get_local_path('')
        send = check_hdc_cmd(f'file send {path_local} /data/local/tmp/')
        pid2 = get_hdcd_pid()
        fds2 = get_hdcd_fd_num()
        check_hdc_cmd('shell rm -rf /storage/largefile')
        assert pid1 != None
        assert pid1 == pid2
        assert fds1 == fds2
        assert send == False
