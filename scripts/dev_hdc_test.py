#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2021 Huawei Device Co., Ltd.
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

# 0. 运行环境: python 3.10+, pytest
# 1. 修改 GP 中字段
# 2. pytest [-k case_name_pattern]
#    eg. pytest -k file 执行方法名含 file 的用例

import subprocess
import os
import hashlib
import time


class GP():
    """ Global Parameters

    customize here !!!
    """
    hdc_head = "hdc.exe -l1"
    local_path = "D:\\hdc_test_resource"
    remote_path = "/data/local/tmp"
    remote_ip = "auto"
    remote_port = 8710


def _get_local_md5(local):
    md5_hash = hashlib.md5()
    with open(local, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            md5_hash.update(byte_block)
    return md5_hash.hexdigest()


def check_shell(cmd, pattern=None, fetch=False):
    cmd = f"{GP.hdc_head} {cmd}"
    print(f"\nexecuting command: {cmd}")
    if pattern: # check output valid
        output = subprocess.check_output(cmd.split()).decode()
        res = pattern in output
        print(f"--> pattern {pattern} {'FOUND' if res else 'NOT FOUND'} in output")
        return res
    elif fetch:
        output = subprocess.check_output(cmd.split()).decode()
        print(f"--> output: {output}")
        return output
    else: # check cmd run successfully
        return subprocess.check_call(cmd.split()) == 0


def _check_file(local, remote):
    cmd = f"shell md5sum {remote}"
    local_md5 = _get_local_md5(local)
    return check_shell(cmd, local_md5)


def _check_app_installed(bundle, is_shared=False):
    dump = "dump-shared" if is_shared else "dump"
    cmd = f"shell bm {dump} -a"
    return check_shell(cmd, bundle)


def file_send(local, remote):
    local_path = os.path.join(GP.local_path, local)
    remote_path = f"{GP.remote_path}/{remote}"
    cmd = f"file send {local_path} {remote_path}"
    assert check_shell(cmd)
    assert _check_file(local_path, remote_path)


def file_recv(remote, local):
    local_path = os.path.join(GP.local_path, local)
    remote_path = f"{GP.remote_path}/{remote}"
    cmd = f"file recv {remote_path} {local_path}"
    assert check_shell(cmd)
    assert _check_file(local_path, remote_path)


def app_install(app, bundle, args=""):
    app = os.path.join(GP.local_path, app)
    install_cmd = f"install {args} {app}"
    assert check_shell(install_cmd, "successfully")
    assert _check_app_installed(bundle, "s" in args)


def app_uninstall(bundle, args=""):
    uninstall_cmd = f"uninstall {args} {bundle}"
    assert check_shell(uninstall_cmd, "successfully")
    assert not _check_app_installed(bundle, "s" in args)


def check_hdc_cmd(cmd, pattern=None, **args):
    if cmd.startswith("file"):
        assert check_shell(cmd, "FileTransfer finish")
        if cmd.startswith("file send"):
            local, remote = cmd.split()[-2:]
        else:
            remote, local = cmd.split()[-2:]
        assert _check_file(local, remote)

    elif cmd.startswith("install"):
        bundle = args["bundle"]
        opt = " ".join(cmd.split()[1:-1])
        assert check_shell(cmd, "successfully")
        assert _check_app_installed(bundle, "s" in opt)

    elif cmd.startswith("uninstall"):
        bundle = cmd.split()[-1]
        opt = " ".join(cmd.split()[1:-1])
        assert check_shell(cmd, "successfully")
        assert not _check_app_installed(bundle, "s" in opt)

    else:
        return check_shell(cmd, pattern, **args)


class TestCommands:
    """
    Usage:

    check_hdc_cmd(cmd, **args):
        1. cmd = file send / file recv: execute and check if md5 of local and remote matches after transfer
            执行file命令 验证两端md5
            eg. check_hdc_cmd("file send D:\somefile.log /data/local/tmp/test.log")
        2. cmd = install, with "bundle" in args: execute and check if bundle added after execution
            执行install命令 验证bundle添加
            eg. check_hdc_cmd("install D:\somepack.hap", bundle="com.hmos.test")
        3. cmd = uninstall: execute and check if bundle removed after execution
            执行uninstall命令 验证bundle删除
            eg. check_hdc_cmd("uninstall -s com.hmos.test")
        4. other cmd: check if execution returns 0
            其他命令 返回值为命令retcode
            eg. check_hdc_cmd("tmode port 6666")

    check_hdc_cmd(cmd, pattern):
        check if <pattern> string in stdout of cmd execution
            其他命令 验证pattern是否在stdout中
            eg. check_hdc_cmd("target mount", "Mount finish")

    check_hdc_cmd(cmd, fetch=True):
        return stdout of cmd execution
            其他命令 返回stdout
            eg. devices = check_hdc_cmd("list targets", fetch=True)

    """

    def test_file_send(self):
        check_hdc_cmd(f"file send {os.path.join(GP.local_path, 'hdc.log')} {GP.remote_path}/hdc.log")

    def test_hap_install(self):
        check_hdc_cmd(f"install -r {os.path.join(GP.local_path, 'entry-default-signed-debug.hap')}", bundle="com.hmos.diagnosis")

    def test_file_cmd(self):
        file_send("hdc.log", "hdc.log")
        file_recv("hdc.log", "test.log")

    def test_app_cmd(self):
        app_install("entry-default-signed-debug.hap", "com.hmos.diagnosis")
        app_uninstall("com.hmos.diagnosis")

        app_install("entry-default-signed-debug.hap", "com.hmos.diagnosis", "-r")
        app_uninstall("com.hmos.diagnosis")

        app_install("analyticshsp-default-signed.hsp", "com.huawei.hms.hsp.analyticshsp", "-s")
        app_uninstall("com.huawei.hms.hsp.analyticshsp", "-s")

    def test_privilege_cmd(self):
        assert check_hdc_cmd("smode -r")
        time.sleep(5)
        assert check_hdc_cmd("shell whoami", "shell")

        assert check_hdc_cmd("smode")
        time.sleep(5)
        assert check_hdc_cmd("shell whoami", "root")

    def test_tcp_cmd(self):
        if not GP.remote_ip: # skip tcp check
            print("!!! remote_ip is none, skip tcp check !!!")
            return
        usb_key = check_hdc_cmd("list targets", fetch=True)
        if GP.remote_ip == "auto":
            ipconf = check_hdc_cmd("shell \"ifconfig -a | grep inet | grep -v 127.0.0.1 | grep -v inet6\"", fetch=True)
            if not ipconf:
                print("!!! device ip not found, skip tcp check !!!")
                return
            GP.remote_ip = ipconf.split(":")[1].split()[0]
            print(f"fetch remote ip: {GP.remote_ip}")
        assert check_hdc_cmd(f"tmode port {GP.remote_port}")
        time.sleep(3)
        assert check_hdc_cmd(f"tconn {GP.remote_ip}:{GP.remote_port}", "Connect OK")

        file_send("hdc.log", "hdc.log")

        assert check_hdc_cmd("tmode usb")
        time.sleep(3)
        assert check_hdc_cmd("list targets", usb_key)

    def test_target_cmd(self):
        check_hdc_cmd("target boot")
        time.sleep(20)
        assert check_hdc_cmd("target mount", "Mount finish")

    def test_version_cmd(self):
        assert check_hdc_cmd("-v", "Ver: 1.3.0a")
        assert check_hdc_cmd("version", "Ver: 1.3.0a")
        assert check_hdc_cmd("checkserver", "Ver: 1.3.0a")

    def test_port_cmd(self):
        fport = "tcp:5555 tcp:5556"
        rport = "tcp:6666 tcp:6667"

        assert check_hdc_cmd(f"fport {fport}", "Forwardport result:OK")
        assert check_hdc_cmd("fport ls", fport)

        assert check_hdc_cmd(f"rport {rport}", "Forwardport result:OK")
        assert check_hdc_cmd("fport ls", rport)

        assert check_hdc_cmd(f"fport rm {fport}", "success")
        assert not check_hdc_cmd("fport ls", fport)
        assert check_hdc_cmd(f"fport rm {rport}", "success")
        assert not check_hdc_cmd("fport ls", rport)
