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
import pytest

from utils import GP, check_hdc_cmd, check_hdc_targets, check_soft_local, check_soft_remote,\
    check_shell, rmdir, check_version, get_local_path, get_remote_path, make_multiprocess_file, load_gp


def clear_env():
    check_hdc_cmd("shell rm -rf data/local/tmp/it_*")
    check_hdc_cmd("shell mkdir data/local/tmp/it_send_dir")


def list_targets():
    assert check_hdc_targets()


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
        assert check_hdc_cmd(f"file send -z {get_local_path(local_path)} {get_remote_path(remote_path)}")
        assert check_hdc_cmd(f"file recv -z {get_remote_path(remote_path)} {get_local_path(f'{local_path}_recv')}")   


class TestFileBase:
    base_file_table = [
        ("empty", "it_empty"),
        ("small", "it_small"),
        ("medium", "it_medium"),
        ("large", "it_large"),
        ("word_100M.txt", "word_100M")
    ]

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    @pytest.mark.parametrize("local_path, remote_path", base_file_table)
    def test_empty_file(self, local_path, remote_path):
        clear_env()
        assert check_hdc_cmd(f"file send {get_local_path(local_path)} {get_remote_path(remote_path)}")
        assert check_hdc_cmd(f"file recv {get_remote_path(remote_path)} {get_local_path(f'{local_path}_recv')}")   


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
        assert check_hdc_cmd(f"file send "
                            f"{get_local_path(f'{test_item}')} {self.remote_unexist_path}/")
        assert check_hdc_cmd(f"file recv {self.remote_unexist_path}/{test_item} "
                            f"{local_unexist_path}{os.sep}")


class TestFileNoExistBundelPath:
    remote_unexist_path = f"{get_remote_path('it_no_exist/deep_test_dir/')}"
    data_storage_el2_path = "data/storage/el2/base"

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
        assert check_soft_local(get_local_path('small'), get_local_path('soft_small'), 
                                get_remote_path('it_small_soft'))
        assert check_soft_remote('it_small_soft', get_remote_path('it_soft_small'),
                                get_local_path('recv_soft_small'))


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
        (f"file send -b {GP.debug_app} {get_local_path('empty_dir')} {get_remote_path('it_empty_dir')}",
        "the source folder is empty"),
        
        # 测试缺少本地和远程路径
        (f"file send -b {GP.debug_app}", "There is no local and remote path"),
        (f"file recv -b {GP.debug_app}", "There is no local and remote path"),
        
        # 测试错误优先级
        (f"file send -b ./{GP.debug_app}", "There is no local and remote path"),
        (f"file recv -b ./{GP.debug_app}", "There is no local and remote path"),
        
        # 测试缺少 bundle 参数
        (f"file recv -b", "[E005003]"),
        (f"file send -b", "[E005003]"),
        
        # 测试本地和远程路径错误
        (f"file send -b ./{GP.debug_app} {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),
        (f"file recv -b ./{GP.debug_app} {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),
        
        # 测试无效的 bundle 参数
        (f"file send -b ./{GP.debug_app} {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),
        (f"file send -b com.AAAA {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),
        (f"file send -b 1 {get_local_path('small')} {get_remote_path('it_small')}", "[E005101]"),
        (f"file send -b {get_local_path('small')} {get_remote_path('it_small')}", "There is no remote path"),
        
        # 测试远程路径错误
        (f"file recv -b ./{GP.debug_app} {get_remote_path('it_small')} {get_local_path('small_recv')}", "[E005101]"),
        
        # 测试路径逃逸
        (f"file send -b {GP.debug_app} {get_local_path('small')} ../../../it_small", "[E005102]"),
        (f"file recv -b {GP.debug_app} ../../../it_small {get_local_path('small_recv')}", "[E005102]"),
    ]
    outside_error = "[E005102]"
    outside_table = [
        # bundle根目录 逃逸
        ("..", True),
        ("../..", True),
        ("../../..", True),
        ("../../../..", True),
        ("../.", True),
        ("./..", True),
        ("../", True),
        (f"../{GP.debug_app}_extra/{data_storage_el2_path}/", True),
        (f"../{GP.debug_app}_notexsit/{data_storage_el2_path}/", True),
        (f"../{GP.debug_app}_notexsit/", True),
        (f"./../../../../../../../../../../aa", True),
        
        # bundle根目录 未逃逸
        (".", False),
        ("./", False),
        ("./.", False),
        ("...", False),
        (f"../../../../../../../../../../../mnt/debug/100/debug_hap/{GP.debug_app}/", False),
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
    @pytest.mark.parametrize("command, expected", error_table)
    def test_file_option_bundle_error_parametrized(self, command, expected):
        assert check_shell(command, expected)

    @pytest.mark.parametrize("remote_path, is_outside", outside_table)
    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    def test_file_option_bundle_check_outside_parametrized(self, remote_path, is_outside):
        if is_outside:
            assert check_shell(f"file send -b {GP.debug_app} {get_local_path('small')} {remote_path}",
                               self.outside_error)
        else:
            assert not check_shell(f"file send -b {GP.debug_app} {get_local_path('small')} {remote_path}",
                                   self.outside_error)