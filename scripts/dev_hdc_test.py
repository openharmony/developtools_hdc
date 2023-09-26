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

import pytest


class GP():
    """ Global Parameters

    customize here !!!
    """
    hdc_exe = "hdc"
    local_path = "/data/resource"
    remote_path = "/data/local/tmp"
    remote_ip = "auto"
    remote_port = 8710
    hdc_head = "hdc"
    device_name = ""
    targets = []
    tmode = "usb"

    @classmethod
    def print_options(cls):
        try:
            targets = subprocess.check_output(f"{cls.hdc_exe} list targets".split()).split()
            cls.device_name = targets[0]
        except (OSError, IndexError):
            targets = [b"failed to auto detect device"]
        cls.targets = [t.decode() for t in targets]
        cls.device_name = cls.targets[0]
        cls.hdc_head = f"{cls.hdc_exe} -t {cls.device_name}"

        info = "HDC Tester Default Options: \n\n" \
        + f"{'hdc execution'.rjust(20, ' ')}: {cls.hdc_head}\n" \
        + f"{'local storage path'.rjust(20, ' ')}: {cls.local_path}\n" \
        + f"{'remote storage path'.rjust(20, ' ')}: {cls.remote_path}\n" \
        + f"{'remote ip'.rjust(20, ' ')}: {cls.remote_ip}\n" \
        + f"{'remote port'.rjust(20, ' ')}: {cls.remote_port}\n" \
        + f"{'device name'.rjust(20, ' ')}: {cls.device_name}\n" \
        + f"{'connect type'.rjust(20, ' ')}: {cls.tmode}\n"

        print(info)

    @classmethod
    def tconn_tcp(cls):
        res = subprocess.check_output(f"{cls.hdc_exe} tconn {cls.remote_ip}:{cls.remote_port}".split()).decode()
        if "Connect OK" in res:
            return True

    @classmethod
    def set_options(cls):
        if opt := input(f"Default hdc execution? [{cls.hdc_head}]\n").strip():
            cls.hdc_head = opt
        if opt := input(f"Default local storage path? [{cls.local_path}]\n").strip():
            cls.local_path = opt
        if opt := input(f"Default remote storage path? [{cls.remote_path}]\n").strip():
            cls.remote_path = opt
        if opt := input(f"Default remote ip? [{cls.remote_ip}]\n").strip():
            cls.remote_ip = opt
        if opt := input(f"Default remote port? [{cls.remote_port}]\n").strip():
            cls.remote_port = int(opt)
        if opt := input(f"Default device name? [{cls.device_name}], opts: {cls.targets}").strip():
            cls.device_name = opt
        if opt := input(f"Default connect type? [{cls.tmode}], opt: [usb, tcp]").strip():
            cls.tmode = opt
        if cls.tmode == "usb":
            cls.hdc_head = f"{cls.hdc_exe} -t {cls.device_name}"
        elif cls.tconn_tcp():
            cls.hdc_head = f"{cls.hdc_exe} -t {cls.remote_ip}:{cls.remote_port}"
        else:
            print(f"tconn {cls.remote_ip}:{cls.remote_port} failed")
            return False
        return True

def _local_path(path):
    return os.path.join(GP.local_path, path)

def _remote_path(path):
    return f"{GP.remote_path}/{path}"

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


def check_file_send(local, remote):
    local_path = os.path.join(GP.local_path, local)
    remote_path = f"{GP.remote_path}/{remote}"
    cmd = f"file send {local_path} {remote_path}"
    return check_shell(cmd) and _check_file(local_path, remote_path)


def check_file_recv(remote, local):
    local_path = os.path.join(GP.local_path, local)
    remote_path = f"{GP.remote_path}/{remote}"
    cmd = f"file recv {remote_path} {local_path}"
    return check_shell(cmd) and _check_file(local_path, remote_path)


def check_app_install(app, bundle, args=""):
    app = os.path.join(GP.local_path, app)
    install_cmd = f"install {args} {app}"
    return check_shell(install_cmd, "successfully") and _check_app_installed(bundle, "s" in args)


def check_app_uninstall(bundle, args=""):
    uninstall_cmd = f"uninstall {args} {bundle}"
    return check_shell(uninstall_cmd, "successfully") and not _check_app_installed(bundle, "s" in args)


def check_hdc_cmd(cmd, pattern=None, **args):
    if cmd.startswith("file"):
        if not check_shell(cmd, "FileTransfer finish"):
            return False
        if cmd.startswith("file send"):
            local, remote = cmd.split()[-2:]
        else:
            remote, local = cmd.split()[-2:]
        return _check_file(local, remote)

    elif cmd.startswith("install"):
        bundle = args.get("bundle", "invalid")
        opt = " ".join(cmd.split()[1:-1])
        return check_shell(cmd, "successfully") and _check_app_installed(bundle, "s" in opt)

    elif cmd.startswith("uninstall"):
        bundle = cmd.split()[-1]
        opt = " ".join(cmd.split()[1:-1])
        return check_shell(cmd, "successfully") and not _check_app_installed(bundle, "s" in opt)

    else:
        return check_shell(cmd, pattern, **args)


def switch_usb():
    res = check_hdc_cmd("tmode usb")
    time.sleep(3)
    if res:
        GP.hdc_head = f"{GP.hdc_exe} -t {GP.device_name}"
    return res

def switch_tcp():
    if not GP.remote_ip: # skip tcp check
        print("!!! remote_ip is none, skip tcp check !!!")
        return True
    if GP.remote_ip == "auto":
        ipconf = check_hdc_cmd("shell \"ifconfig -a | grep inet | grep -v 127.0.0.1 | grep -v inet6\"", fetch=True)
        if not ipconf:
            print("!!! device ip not found, skip tcp check !!!")
            return True
        GP.remote_ip = ipconf.split(":")[1].split()[0]
        print(f"fetch remote ip: {GP.remote_ip}")
    check_hdc_cmd(f"tmode port {GP.remote_port}")
    time.sleep(3)
    res = check_hdc_cmd(f"tconn {GP.remote_ip}:{GP.remote_port}", "Connect OK")
    if res:
        GP.hdc_head = f"{GP.hdc_exe} -t {GP.remote_ip}:{GP.remote_port}"
    return res

class TestCommands:
    """Usage:

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

    def test_empty_file(self):
        assert check_hdc_cmd(f"file send {_local_path('empty')} {_remote_path('it_empty')}")
        assert check_hdc_cmd(f"file recv {_remote_path('it_empty')} {_local_path('empty_recv')}")

    def test_small_file(self):
        assert check_hdc_cmd(f"file send {_local_path('small')} {_remote_path('it_small')}")
        assert check_hdc_cmd(f"file recv {_remote_path('it_small')} {_local_path('small_recv')}")

    def test_large_file(self):
        assert check_hdc_cmd(f"file send {_local_path('large')} {_remote_path('it_large')}")
        assert check_hdc_cmd(f"file recv {_remote_path('it_large')} {_local_path('large_recv')}")

    def test_hap_install(self):
        assert check_hdc_cmd(f"install -r {_local_path('entry-default-signed-debug.hap')}",
                             bundle="com.hmos.diagnosis")

    def test_app_cmd(self):
        assert check_app_install("entry-default-signed-debug.hap", "com.hmos.diagnosis")
        assert check_app_uninstall("com.hmos.diagnosis")

        assert check_app_install("entry-default-signed-debug.hap", "com.hmos.diagnosis", "-r")
        assert check_app_uninstall("com.hmos.diagnosis")

        assert check_app_install("analyticshsp-default-signed.hsp", "com.huawei.hms.hsp.analyticshsp", "-s")
        assert check_app_uninstall("com.huawei.hms.hsp.analyticshsp", "-s")

    def test_smode(self):
        assert check_hdc_cmd("smode -r")
        time.sleep(5)
        assert check_hdc_cmd("shell whoami", "shell")

        assert check_hdc_cmd("smode")
        time.sleep(5)
        assert check_hdc_cmd("shell whoami", "root")

    def test_tmode(self):
        if GP.tmode == "usb":
            assert switch_tcp()
            assert switch_usb()
        else:
            assert switch_usb()
            assert switch_tcp()

    def test_target_cmd(self):
        check_hdc_cmd("target boot")
        time.sleep(20)
        assert check_hdc_cmd("target mount", "Mount finish")

    def test_version_cmd(self):
        assert check_hdc_cmd("-v", "Ver: 1.3.0a")
        assert check_hdc_cmd("version", "Ver: 1.3.0a")
        assert check_hdc_cmd("checkserver", "Ver: 1.3.0a")

    def test_fport_cmd(self):
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

    def setup_class(self):
        print("setting up env ...")
        # check_hdc_cmd("tmode usb")
        # check_hdc_cmd("smode")
        check_hdc_cmd("shell rm -rf /data/local/tmp/it_*")

    def teardown_class(self):
        pass

def select_cmd():
    msg = "1) Proceed tester\n" \
        + "2) Customize tester\n" \
        + "3) Setup files for transfer\n" \
        + "4) Cancel\n" \
        + ">> "

    while True:
        opt = input(msg).strip()
        if len(opt) == 1 and '1' <= opt <= '4':
            return opt

def prepare_source():

    def gen_file(path, size):
        index = 0
        path = os.path.abspath(path)
        with open(path, 'w') as f:
            while index < size:
                f.write(hashlib.md5(str(time.time_ns()).encode()).hexdigest())
                index += 64

    print("generating empty file ...")
    gen_file(os.path.join(GP.local_path, "empty"), 0)

    print("generating small file ...")
    gen_file(os.path.join(GP.local_path, "small"), 102400)

    print("generating large file ...")
    gen_file(os.path.join(GP.local_path, "large"), 2 * 1024 ** 3)

    print("generating dir with small file ...")
    dir_path = os.path.join(GP.local_path, "normal_dir")
    subprocess.call(f"rm -rf {dir_path}".split())
    subprocess.call(f"mkdir -p {dir_path}".split())
    gen_file(os.path.join(dir_path, "small2"), 102400)

    print("generating empty dir ...")
    dir_path = os.path.join(GP.local_path, "empty_dir")
    subprocess.call(f"rm -rf {dir_path}".split())
    subprocess.call(f"mkdir -p {dir_path}".split())



def setup_tester():
    while True:
        GP.print_options()
        opt = int(select_cmd())
        if opt == 1:
            return True
        elif opt == 2:
            if not GP.set_options():
                return False
        elif opt == 3:
            prepare_source()
        else:
            return False


if __name__ == "__main__":

    if setup_tester():
        print("starting test, plz ensure hap / hsp is in local storage path")
        msg = "input test case name pattern [file / app / target / fport / ...], blank for all cases\n>> "
        pattern = input(msg).strip()
        if pattern:
            pytest.main(["-s", "-k", pattern])
        else:
            pytest.main(["-s"])
