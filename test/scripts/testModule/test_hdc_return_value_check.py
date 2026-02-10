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
import logging
import os
import re
import time
import pytest

from utils import GP, get_shell_result, get_cmd_block_output, get_end_symbol, \
    run_command_with_timeout, check_shell, get_remote_path, get_local_path, \
    load_gp, check_unsupport_systems

logger = logging.getLogger(__name__)


class TestHdcReturnValue:
    @staticmethod
    def check_track_jpid():
        result = get_cmd_block_output("hdc track-jpid -a", timeout=2)
        result = result.split('\n')
        content_size = 0  # 所有表示长度的加起来
        first_line_size = 0  # 所有表示长度的内容长度之和
        size = 0
        for line in result:
            size = size + len(line) + 1

            if " " in line:  # 数据行
                print("data row, line size all size ", content_size, size)
            elif len(line) == 0:  # 最后一行
                size = size - 1
                print("last row", size)
            else:
                first_line_size = first_line_size + len(line) + 1
                line = line.replace(get_end_symbol(), "")
                line = line.replace("\n", "")
                content_size = content_size + int(line, 16)

        return size == first_line_size + content_size

    """
    hdc help命令的输出与hdc.log文件内容非空行逐行比较，全部相等则成功
    """

    @pytest.mark.L0
    @check_unsupport_systems(["Linux", "Harmony", "Darwin"])
    def test_hdc_help(self):
        result = get_shell_result(f"help")
        result_lines = re.split("\r|\n", result)
        help_file = f"help.log"
        index = 0
        with open(help_file, 'r') as file:
            for line in file.readlines():
                line = line.replace("\n", "")
                while len(result_lines[index]) == 0:
                    index = index + 1
                    if index >= len(result_lines):
                        return
                if line:
                    if line != result_lines[index]:
                        logger.warning(f"line:{line}, result line:{result_lines[index]}")
                        assert False
                    index = index + 1
        return

    @pytest.mark.L0
    @check_unsupport_systems(["Linux", "Harmony", "Darwin"])
    def test_hdc_help_verbose(self):
        result = get_shell_result(f"-h verbose")
        result_lines = re.split("\r|\n", result)
        help_file = f"help_verbose.log"
        index = 0
        with open(help_file, 'r') as file:
            for line in file.readlines():
                line = line.replace("\n", "")
                while len(result_lines[index]) == 0:
                    index = index + 1
                    if index >= len(result_lines):
                        return
                if line:
                    if line != result_lines[index]:
                        logger.warning(f"line:{line}, result line:{result_lines[index]}")
                        assert False
                    index = index + 1
        return

    """
    格式形如：    Ver: 3.1.0e
    """

    @pytest.mark.L0
    def test_hdc_version(self):
        result = get_shell_result(f"-v")
        result = re.sub("[\r\n]", "", result)
        pattern = r'Ver: \d+.\d+.\d+([A-Za-z]+)'
        match = re.match(pattern, result)
        assert match is not None

    """
    hdc list targets
    返回值格式如下：
    1）无设备， 返回[Empty]
    2) 有设备, 分行显示 usb/uart设备是常规字符串 tcp设备是ip地址：port端口号
    """

    @pytest.mark.L0
    def test_hdc_list_targets(self):
        result = get_shell_result(f"list targets")
        result = re.split("\r|\n", result)
        if result[0] == "[Empty]":
            return
        for item in result:
            if item:
                pattern = r'([A-Za-z0-9]+)'
                match_usb = re.fullmatch(pattern, item)
                pattern_tcp = r'\d+.\d+.\d+.\d+:\d+'
                match_tcp = re.fullmatch(pattern_tcp, item)
                assert match_usb is not None or match_tcp is not None

    """
    hdc start [-r]
    无返回
    """

    @pytest.mark.L0
    def test_hdc_start(self):
        assert check_shell(f"start", "")
        assert check_shell(f"start -r", "")

    """
    hdc target mount
    返回 Mount finish
    """

    @pytest.mark.L0
    def test_hdc_target_mount(self):
        ret1 = check_shell(f"target mount", "Mount finish")
        ret2 = check_shell(f"target mount", "[Fail]Operate need running as root")
        assert ret1 or ret2

    """
    hdc wait
    返回 空
    """

    @pytest.mark.L0
    def test_hdc_wait(self):
        check_shell(f"kill")
        assert check_shell(f"{GP.hdc_head} wait", "")

    """
    hdc target boot [-bootloader] [-recovery]
    返回 空
    """

    @pytest.mark.L0
    def test_hdc_target_boot(self):
        assert check_shell(f"target boot", "")
        time.sleep(20)
        # run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        # assert check_shell(f"target boot -bootloader", "")
        # run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        # assert check_shell(f"target boot -recovery", "")

    """
    hdc smode [-r]
    返回 空
    """

    @pytest.mark.L0
    def test_hdc_smode(self):
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        assert check_shell(f"smode -r", "")
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        assert check_shell(f"smode", "")

    """
    hdc tmode [usb] | [port XXXX]
    返回 空
    """

    @pytest.mark.L0
    def test_hdc_tmode(self):
        time.sleep(10)
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        assert check_shell(f"tmode usb", "[Fail][E001000]For USB debugging, please set it on the device's Settings UI")
        assert check_shell(f"tmode port 11111", "Set device run mode successful.")

    """
    hdc file send [local_path] [remote_path]
    """

    @pytest.mark.L0
    def test_hdc_file(self):
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        result = get_shell_result(f"file send {get_local_path('small')} {get_remote_path('test')}")
        result = result.replace(get_end_symbol(), "")
        pattern = r'FileTransfer finish, Size:(\d+), File count = (\d+), time:(\d+)ms rate:(\d+.\d+)kB/s'
        match_send = re.match(pattern, result)
        assert match_send is not None

        assert check_shell(f"file send {get_local_path('small')}", "[Fail]There is no remote path")
        assert check_shell(f"file send {get_local_path('small')} abc",
                           "[Fail]Error opening file: read-only file system, path:abc")

        result = get_shell_result(f"file send {get_local_path('small_no_exsit')} {get_remote_path('test')}")
        result = result.split(get_end_symbol())
        pattern = r'\[Fail\]Error opening file: no such file or directory, path:(.*)small_no_exsit'
        match = re.match(pattern, result[0])
        assert match is not None
        pattern = r'\[F\]\[(.*)\] open path:(.*)small_no_exsit, localPath:(.*)small_no_exsit, error:no such file or ' \
                  r'directory, dir:0, master:1'
        match = re.match(pattern, result[1])
        assert match is not None

        assert check_shell(f"file send -b com.package.unknown {get_local_path('small')}",
                           "[Fail]There is no remote path")
        assert check_shell(f"file send -b com.package.unknown {get_local_path('small')} remote_path",
                           "[Fail][E005101] Invalid bundle name: com.package.unknown")

        check_shell(f"smode -r")
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        result = get_shell_result(f"file send {get_local_path('small')} /system/lib/")
        result = result.replace(get_end_symbol(), "")
        result = result.replace("\r", "")
        assert (result == "[Fail]Error opening file: permission denied, path:/system/lib/small" or
                result == "[Fail]Error opening file: read-only file system, path:/system/lib/small")

        check_shell(f"smode")
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)

    """
    hdc fport tcp:xxxx tcp:xxxx
    """

    @pytest.mark.L0
    def test_hdc_fport(self):
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        assert check_shell(f"fport tcp:12345 tcp:23456", "Forwardport result:OK")
        assert check_shell(f"fport tcp:12345 tcp:23456", "[Fail]TCP Port listen failed at 12345")
        assert check_shell(f"fport ls", f"{GP.device_name}    tcp:12345 tcp:23456    [Forward]")
        assert check_shell(f"fport rm tcp:12345 tcp:23456", "Remove forward ruler success, ruler:tcp:12345 tcp:23456")

        assert check_shell(f"rport tcp:12345 tcp:23456", "Forwardport result:OK")
        assert check_shell(f"rport tcp:12345 tcp:23456", "[Fail]TCP Port listen failed at 12345")
        assert check_shell(f"fport ls", f"{GP.device_name}    tcp:12345 tcp:23456    [Reverse]")
        assert check_shell(f"fport rm tcp:12345 tcp:23456", "Remove forward ruler success, ruler:tcp:12345 tcp:23456")
        assert check_shell(f"fport rm tcp:1235 tcp:2346", "[Fail]Remove forward ruler failed, ruler is not exist "
                                                          "tcp:1235 tcp:2346")

        assert check_shell(f"fport tcp:2222222 tcp:22213", "[Fail]Forward parament failed")

    """
    hdc install xxx.hap
    """

    @pytest.mark.L0
    def test_hdc_install(self):
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        result = get_shell_result(f"install {get_local_path('AACommandpackage.hap')}")
        result = re.sub("[\r\n]", "", result)
        pattern = r'\[Info\]App install path:(.*)AACommandpackage\.hap msg:install bundle successfully\. AppMod finish'
        match_install = re.match(pattern, result)
        assert match_install is not None

        result = get_shell_result(f"uninstall com.example.actsaacommandtestatest")
        result = re.sub("[\r\n]", "", result)
        result = result.replace("\r", "")
        pattern = r'\[Info\]App uninstall path: msg:uninstall bundle successfully\. AppMod finish'
        match_uninstall = re.match(pattern, result)
        assert match_uninstall is not None

        result = get_shell_result(f"install {get_local_path('hap_no_exsit.hap')} {get_remote_path('test')}")
        result = re.split(get_end_symbol(), result)
        pattern = r'\[Fail\]Error opening file: no such file or directory, path:(.*)hap_no_exsit.hap'
        match = re.match(pattern, result[0])
        assert match is not None
        pattern = r'\[F\]\[(.*)\] open path:(.*)hap_no_exsit.hap, localPath:(.*)hap_no_exsit.hap, error:no such file ' \
                  r'or directory, dir:0, master:1'
        match = re.match(pattern, result[1])
        assert match is not None

        result = get_shell_result(f"uninstall com.example.not_exsit")
        result = re.sub("[\r\n]", "", result)
        result = result.replace("\r", "")
        pattern = r'\[Info\]App uninstall path: msg:error: failed to uninstall bundle\. code:9568386 error: uninstall ' \
                  r'missing installed bundle\. AppMod finish'
        match_uninstall = re.match(pattern, result)
        assert match_uninstall is not None

        assert check_shell(f"install not_hap_file", "[Fail][E006001] Not any installation package was found")

    """
    hdc install "-r; touch/data/local/tmp/test001" xxx.hap
    """
    @pytest.mark.L0
    def test_hdc_install_inject(self):
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)

        get_shell_result(f"shell rm /data/local/tmp/test001")
        result = get_shell_result(f"install \"-r; touch /data/local/tmp/test001\" {get_local_path('AACommandpackage.hap')}")
        result = result.replace("\r\n", "")
        pattern = r'.*Incorrect package name or option.*'
        match_install = re.match(pattern, result)
        assert match_install is not None
        assert check_shell(f"shell ls /data/local/tmp/test001", "No such file or directory")

    """
    hdc track-jpid
    hdc jpid
    """

    @pytest.mark.L0
    def test_hdc_track_jpid(self):
        assert self.check_track_jpid()

        result = get_shell_result(f"jpid")
        result = re.split("\r|\n", result)
        for item in result:
            if item:
                item = item.replace("\r", "")
                assert item.isdigit()


    """
    hdc shell
    """

    @pytest.mark.L0
    def test_hdc_shell(self):
        check_shell(f"smode")
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)

        result = get_cmd_block_output(f"{GP.hdc_head} shell", 10)
        result = re.sub("[\r\n]", "", result)
        assert result.find("#") > 0

    """
    hdc shell find /nosuchfilename -name nosuchfilename
    """

    @pytest.mark.L0
    def test_hdc_shell_find(self):
        find_cmd = "shell find /nosuchfilename -name nosuchfilename"
        assert check_shell(find_cmd, "No such file or directory")

    """
    hdc shell ls
    """

    @pytest.mark.L0
    def test_hdc_shell_ls(self):
        file_list = []
        for count in range(10):
            file_list.append(f"testfile{count}")
        test_path = "/data/local/tmp/testdir"
        try:
            get_shell_result(f"shell mkdir {test_path}")
            for file_name in file_list:
                file_path = f"{test_path}/{file_name}"
                get_shell_result(f"shell touch {file_path}")
            ls_result = get_shell_result(f"shell ls {test_path}")
            print(f"ls_result:{ls_result}")
            ls_result_list = ls_result.splitlines()
            print(f"ls_result_list:{ls_result_list}")
            assert len(ls_result_list) == len(file_list)
            for file_read_name in ls_result_list:
                assert file_read_name in file_list
        finally:
            rm_result = get_shell_result(f"shell rm -fr {test_path}")
            print(f"finally rm -fr {test_path}, result:{rm_result}")

    """
    hdc tconn [ip]:[port]
    """

    @pytest.mark.L0
    def test_hdc_tconn(self):
        assert check_shell(f"tmode port 7777", "Set device run mode successful.")
        time.sleep(2)
        run_command_with_timeout(f"{GP.hdc_head} wait", 20)
        n = 0
        while True:
            if check_shell(f"fport tcp:{12345 + n} tcp:7777", "Forwardport result:OK"):
                assert check_shell(f"tconn 127.0.0.1:{12345 + n}", "Connect OK")
                assert check_shell(f"list targets", f"127.0.0.1:{12345 + n}")
                assert check_shell(f"kill")
                break
            else:
                n = n + 1

        assert check_shell(f"tconn 127.0.0.1:234234", "[Fail]IP:Port incorrect")
        assert check_shell(f"tconn 12312313.0.0.1:3332", "[Fail][E001104]:IP address incorrect")
        assert check_shell(f"tconn 127.0.0.1:2321", "[Fail]Connect failed")
