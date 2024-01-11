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

# 打包
# pip install pyinstaller
# prepare assert source dir includes your data files
# pyi-makespec  -D --add-data assert:assert dev_hdc_test.py
# pyinstaller dev_hdc_test.spec
# 执行 dev_hdc_test.exe

import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import csv
import pytest
import pkg_resources


class GP(object):
    """ Global Parameters

    customize here !!!
    """
    hdc_exe = "hdc"
    local_path = "resource"
    remote_path = "data/local/tmp"
    remote_ip = "auto"
    remote_port = 8710
    hdc_head = "hdc"
    device_name = ""
    targets = []
    tmode = "usb"
    changed_testcase = "n"
    testcase_path = "ts_windows.csv"
    loaded_testcase = 0

    @classmethod
    def init(cls):
        if os.path.exists(".hdctester.conf"):
            cls.load()
            return
        else:
            cls.set_options()
            cls.print_options()
            cls.dump()
        try:
            targets = subprocess.check_output(f"{cls.hdc_exe} list targets".split()).split()
        except (OSError, IndexError):
            targets = [b"failed to auto detect device"]
        cls.targets = [t.decode() for t in targets]
        if len(cls.targets) > 1:
            print("Multiple device detected, please select one:")
            for i, t in enumerate(cls.targets):
                print(f"{i+1}. {t}")
            print("input the nums of the device above:")
            cls.device_name = cls.targets[int(input()) - 1]
        else:
            cls.device_name = cls.targets[0]
        if cls.device_name == "failed to auto detect device":
            print("No device detected, please check your device connection")
            return
        if cls.device_name == "":
            cls.device_name = subprocess.run(['hdc', 'list', 'targets'],
                                              stdout=subprocess.PIPE).stdout.decode('utf-8').strip()
        elif cls.device_name == "[empty]":
            print("No hdc device detected.")
            return
        cls.hdc_head = f"{cls.hdc_exe} -t {cls.device_name}"
        return


    @classmethod
    def dump(cls):
        try:
            os.remove(".hdctester.conf")
        except OSError:
            pass
        content = filter(lambda k: not k[0].startswith("__") and not type(k[1]) == classmethod, cls.__dict__.items())
        json_str = json.dumps(dict(content))
        fd = os.open(".hdctester.conf", os.O_WRONLY | os.O_CREAT, 0o755)
        os.write(fd, json_str.encode())
        os.close(fd)
        return True


    @classmethod
    def load(cls):
        with open(".hdctester.conf") as f:
            content = json.load(f)
            cls.hdc_exe = content.get("hdc_exe")
            cls.local_path = content.get("local_path")
            cls.remote_path = content.get("remote_path")
            cls.remote_ip = content.get("remote_ip")
            cls.hdc_head = content.get("hdc_head")
            cls.tmode = content.get("tmode")
            cls.device_name = content.get("device_name")
            cls.changed_testcase = content.get("changed_testcase")
            cls.testcase_path = content.get("testcase_path")
            cls.loaded_testcase = content.get("load_testcase")
        return True


    @classmethod
    def print_options(cls):
        info = "HDC Tester Default Options: \n\n" \
        + f"{'hdc execution'.rjust(20, ' ')}: {cls.hdc_exe}\n" \
        + f"{'local storage path'.rjust(20, ' ')}: {cls.local_path}\n" \
        + f"{'remote storage path'.rjust(20, ' ')}: {cls.remote_path}\n" \
        + f"{'remote ip'.rjust(20, ' ')}: {cls.remote_ip}\n" \
        + f"{'remote port'.rjust(20, ' ')}: {cls.remote_port}\n" \
        + f"{'device name'.rjust(20, ' ')}: {cls.device_name}\n" \
        + f"{'connect type'.rjust(20, ' ')}: {cls.tmode}\n" \
        + f"{'hdc head'.rjust(20, ' ')}: {cls.hdc_head}\n" \
        + f"{'changed testcase'.rjust(20, ' ')}: {cls.changed_testcase}\n" \
        + f"{'testcase path'.rjust(20, ' ')}: {cls.testcase_path}\n" \
        + f"{'loaded testcase'.rjust(20, ' ')}: {cls.loaded_testcase}\n"
        print(info)


    @classmethod
    def tconn_tcp(cls):
        res = subprocess.check_output(f"{cls.hdc_exe} tconn {cls.remote_ip}:{cls.remote_port}".split()).decode()
        if "Connect OK" in res:
            return True
        else:
            return False


    @classmethod
    def set_options(cls):
        if opt := input(f"Default hdc execution? [{cls.hdc_exe}]\n").strip():
            cls.hdc_exe = opt
        if opt := input(f"Default local storage path? [{cls.local_path}]\n").strip():
            cls.local_path = opt
        if opt := input(f"Default remote storage path? [{cls.remote_path}]\n").strip():
            cls.remote_path = opt
        if opt := input(f"Default remote ip? [{cls.remote_ip}]\n").strip():
            cls.remote_ip = opt
        if opt := input(f"Default remote port? [{cls.remote_port}]\n").strip():
            cls.remote_port = int(opt)
        if opt := input(f"Default device name? [{cls.device_name}], opts: {cls.targets}\n").strip():
            cls.device_name = opt
        if opt := input(f"Default connect type? [{cls.tmode}], opt: [usb, tcp]\n").strip():
            cls.tmode = opt
        if cls.tmode == "usb":
            if cls.device_name == "":
                cls.device_name = subprocess.run(['hdc', 'list', 'targets'],
                                               stdout=subprocess.PIPE).stdout.decode('utf-8').strip()
            cls.hdc_head = f"{cls.hdc_exe} -t {cls.device_name}"
        elif cls.tconn_tcp():
            cls.hdc_head = f"{cls.hdc_exe} -t {cls.remote_ip}:{cls.remote_port}"
        else:
            print(f"tconn {cls.remote_ip}:{cls.remote_port} failed")
            return False
        return True
    

    @classmethod
    def change_testcase(cls):
        if opt := input(f"Change default testcase?(Y/n) [{cls.changed_testcase}]\n").strip():
            cls.changed_testcase = opt
            if opt == "n":
                return False
        if opt := input(f"Use default testcase path?(Y/n) [{cls.testcase_path}]\n").strip():
            cls.testcase_path =  os.path.join(opt)
        cls.print_options()
        return True
    

    @classmethod
    def load_testcase(cls):
        print("this fuction will coming soon.")
        return False
    

def rmdir(path):
    try:
        if sys.platform == "win32":
            if os.path.isfile(path):
                subprocess.call(f"del {path}".split())
            else:
                subprocess.call(f"del /Q {path}".split())
        else:
            subprocess.call(f"rm -rf {path}".split())
    except OSError:
        pass


def get_local_path(path):
    return os.path.join(GP.local_path, path)


def get_remote_path(path):
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
        print(f"--> pattern [{pattern}] {'FOUND' if res else 'NOT FOUND'} in output")
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

def check_hdc_targets():
    cmd = f"{GP.hdc_head} list targets"
    print(GP.device_name)
    return check_shell(cmd, GP.device_name)


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
    ret = check_hdc_cmd(f"tmode port {GP.remote_port}")
    if ret:
        time.sleep(3)
    res = check_hdc_cmd(f"tconn {GP.remote_ip}:{GP.remote_port}", "Connect OK")
    if res:
        GP.hdc_head = f"{GP.hdc_exe} -t {GP.remote_ip}:{GP.remote_port}"
    return res


def select_cmd():
    msg = "1) Proceed tester\n" \
        + "2) Customize tester\n" \
        + "3) Setup files for transfer\n" \
        + "4) Load custom testcase(default unused) \n" \
        + "5) Exit\n" \
        + ">> "

    while True:
        opt = input(msg).strip()
        if len(opt) == 1 and '1' <= opt <= '5':
            return opt


def prepare_source():

    def gen_file(path, size):
        index = 0
        path = os.path.abspath(path)
        fd = os.open(path, os.O_WRONLY | os.O_CREAT, 0o755)

        while index < size:
            os.write(fd, os.urandom(1024))
            index += 1024
        os.close(fd)

    def gen_word_file(chcp_type):
        path = os.path.join(GP.local_path, f"{chcp_type}.txt")
        path = os.path.abspath(path)
        if chcp_type == "numbers":
            with open(path, "w") as f:
                for i in range(1, 200):
                    nums_str = "1234567890"
                    f.write(nums_str)
        else:
            with open(path, "w", encoding=chcp_type) as f:
                for i in range(1, 0x10FFFF + 1):
                    char = chr(i)
                    f.write(char)

    print(f"in prepare {GP.local_path},wait for 2 mins.")
    current_path = os.getcwd()
    os.mkdir(GP.local_path)
    print("generating empty file ...")
    gen_file(os.path.join(GP.local_path, "empty"), 0)

    print("generating small file ...")
    gen_file(os.path.join(GP.local_path, "small"), 102400)

    print("generating large file ...")
    gen_file(os.path.join(GP.local_path, "large"), 2 * 1024 ** 3)

    print("generating dir with small file ...")
    dir_path = os.path.join(GP.local_path, "normal_dir")
    rmdir(dir_path)
    os.mkdir(dir_path)
    gen_file(os.path.join(dir_path, "small2"), 102400)

    print("generating empty dir ...")
    dir_path = os.path.join(GP.local_path, "empty_dir")
    rmdir(dir_path)
    os.mkdir(dir_path)
    
    if os.path.exists("entry-default-signed-debug.hap"):
        print("copy the hap file to resource dir...")
        shutil.copy("entry-default-signed-debug.hap", GP.local_path)
    else:
        print("No hap File!")
    if os.path.exists("panalyticshsp-default-signed.hsp"):
        shutil.copy("panalyticshsp-default-signed.hsp", GP.local_path)
    else:
        print("No hsp File!")


def setup_tester():
    while True:
        GP.print_options()
        opt = int(select_cmd())
        if opt == 1:
            return True
        elif opt == 2:
            if not GP.set_options():
                return False
            GP.dump()
        elif opt == 3:
            prepare_source()
        elif opt == 4:
            if not GP.load_testcase():
                return False
        elif opt == 5:
            return False
        else:
            return False


def load_testcase():
    if not GP.load_testcase:
        print("load testcase failed")
        return False
    print("load testcase success")
    return True


def check_library_installation(library_name):
    try:
        pkg_resources.get_distribution(library_name)
        return 0
    except pkg_resources.DistributionNotFound:
        print(f"\n\n{library_name} is not installed.\n\n")
        print(f"try to use command below:")
        print(f"pip install {library_name}")
        return 1