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
# 运行环境: python 3.10+, pytest, pytest-repeat, pytest-testreport
# 准备文件：package.zip
# pip install pytest pytest-testreport pytest-repeat
# python hdc_normal_test.py


import argparse
import logging
import os
import time
import pytest


from dev_hdc_test import GP
from dev_hdc_test import check_library_installation
from dev_hdc_test import check_hdc_cmd, check_hdc_targets, get_local_path, get_remote_path
from dev_hdc_test import check_soft_local, check_soft_remote
from dev_hdc_test import check_app_uninstall, prepare_source, make_multiprocess_file
from dev_hdc_test import execute_lines_in_file, hdc_get_key, rmdir, pytest_run


logging.basicConfig(level=logging.INFO,
                format='%(asctime)s %(filename)s [line:%(lineno)d] %(levelname)s %(message)s',
                datefmt='%d %b %Y %H:%M:%S',
                   ) 


def test_list_targets():
    assert check_hdc_targets()


@pytest.mark.repeat(5)
def test_soft_link():
    assert check_soft_local(get_local_path('small'), 
                            get_local_path('soft_small'), 
                            get_remote_path('it_small_soft')
                            )
    assert check_soft_remote('it_small_soft',
                             get_remote_path('it_soft_small'),
                             get_local_path('recv_soft_small')
                            )


@pytest.mark.repeat(1)
def test_mix_file():
    muti_num = 5 # the count of multiprocess file
    assert make_multiprocess_file(get_local_path('small'), get_remote_path('it_small'), 'send', muti_num, "file")
    assert make_multiprocess_file(get_local_path('small_recv'), get_remote_path('it_small'), 'recv', muti_num, "file")
    assert make_multiprocess_file(get_local_path('medium'), get_remote_path('it_medium'), 'send', muti_num, "file")
    assert make_multiprocess_file(get_local_path('medium_recv'), get_remote_path('it_medium'), 'recv', muti_num, "file")



def test_recv_dir():
    assert make_multiprocess_file(get_local_path('package'), get_remote_path(''), 'send', 1, "dir")
    assert check_hdc_cmd(f"shell mv {get_remote_path('package')} {get_remote_path('it_package')}")
    assert make_multiprocess_file(get_local_path(''), get_remote_path('it_package'), 'recv', 1, "dir")
    if os.path.exists(get_local_path('it_package')):
        rmdir(get_local_path('it_package'))


def test_te_case():
    execute_lines_in_file('te.txt')


def test_hap_install():
    assert check_hdc_cmd(f"install -s {get_local_path('libA_v10001.hsp')}",
                            bundle="com.example.liba")

    assert check_hdc_cmd(f"install -s {get_local_path('libB_v10001.hsp')}",
                            bundle="com.example.libb")

    app_name_default_a = "com.example.liba"
    app_name_default_b = "com.example.libb"
    assert check_app_uninstall(app_name_default_a, "-s")
    assert check_app_uninstall(app_name_default_b, "-s")


def test_shell_print():
    check_hdc_cmd("shell echo 'hello world'")


def test_shell_rm():
    check_hdc_cmd("shell rm -rf data/local/tmp/it_*")


def test_shell_ls():
    check_hdc_cmd("shell ls data/local/tmp")


def test_file_smap():
    pid = hdc_get_key("shell pidof hdcd")
    check_hdc_cmd(f"file recv proc/{pid}/smaps resource/smaps")


def test_shell_mkdir():
    check_hdc_cmd("shell mkdir -p data/local/tmp/it")


def test_shell_rmdir():
    check_hdc_cmd("shell rmdir data/local/tmp/it")


def setup_class():
    print("setting up env ...")
    check_hdc_cmd("shell rm -rf data/local/tmp/it_*")
    GP.load()


def teardown_class():
    pass


def run_main():
    if check_library_installation("pytest"):
        exit(1)

    if check_library_installation("pytest-testreport"):
        exit(1)
    
    if check_library_installation("pytest-repeat"):
        exit(1)

    GP.init()
    if not os.path.exists(GP.local_path):
        prepare_source()

    choice_default = ""
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=1,
                        help='test times')
    parser.add_argument('--verbose', '-v', default=__file__,
                        help='filename')
    parser.add_argument('--desc', '-d', default='Test for function.',
                        help='Add description on report')
    args = parser.parse_args()

    pytest_run(args)


if __name__ == "__main__":
    run_main()