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
# 运行环境: python 3.10+, pytest

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import tempfile
import functools
import logging
import socket
import platform
import pytest
import importlib
import random
import string

logger = logging.getLogger(__name__)


class GP(object):
    """ Global Parameters

    customize here !!!
    """
    hdc_exe = "hdc"
    local_path = "resource"
    remote_path = "data/local/tmp"
    remote_dir_path = "data/local/tmp/it_send_dir"
    remote_ip = "auto"
    remote_port = 8710
    hdc_head = "hdc"
    device_name = ""
    targets = []
    tmode = "usb"
    changed_testcase = "n"
    testcase_path = "ts_windows.csv"
    loaded_testcase = 0
    hdcd_rom = "not checked"
    debug_app = "com.example.myapplication"

    @classmethod
    def init(cls):
        if os.path.exists(".hdctester.conf"):
            cls.load()
            cls.start_host()
            cls.list_targets()
        else:
            cls.set_options()
            cls.print_options()
            cls.start_host()
            cls.dump()
        return


    @classmethod
    def start_host(cls):
        cmd = f"{cls.hdc_exe} start"
        res = subprocess.call(cmd.split())
        return res

    @classmethod
    def list_targets(cls):
        try:
            targets = subprocess.check_output(f"{cls.hdc_exe} list targets".split()).split()
        except (OSError, IndexError):
            targets = [b"failed to auto detect device"]
            cls.targets = [targets[0].decode()]
            return False
        cls.targets = [t.decode() for t in targets]
        return True


    @classmethod
    def get_device(cls):
        cls.start_host()
        cls.list_targets()
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
            return False
        elif cls.device_name == "[empty]":
            print("No hdc device detected.")
            return False
        cls.hdc_head = f"{cls.hdc_exe} -t {cls.device_name}"
        return True


    @classmethod
    def dump(cls):
        try:
            os.remove(".hdctester.conf")
        except OSError:
            pass
        content = filter(
            lambda k: not k[0].startswith("__") and not isinstance(k[1], classmethod), cls.__dict__.items())
        json_str = json.dumps(dict(content))
        fd = os.open(".hdctester.conf", os.O_WRONLY | os.O_CREAT, 0o755)
        os.write(fd, json_str.encode())
        os.close(fd)
        return True


    @classmethod
    def load(cls):
        if not os.path.exists(".hdctester.conf"):
            raise ConfigFileNotFoundException("No config file found, please run command [python prepare.py]")
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
            ret = cls.get_device()
            if ret:
                print("USB device detected.")
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
            cls.testcase_path = os.path.join(opt)
        cls.print_options()
        return True
    

    @classmethod
    def load_testcase(cls):
        print("this fuction will coming soon.")
        return False
    
    @classmethod
    def get_version(cls):
        version = f"v1.0.6a"
        return version


def pytest_run():
    start_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(time.time()))
    pytest.main()
    end_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(time.time()))
    report_time = time.strftime('%Y-%m-%d_%H_%M_%S', time.localtime(time.time()))
    report_dir = os.path.join(os.getcwd(), "reports")
    if not os.path.exists(report_dir):
        os.mkdir(report_dir)
    report_file = os.path.join(report_dir, f"{report_time}report.html")
    print(f"Test over, the script version is {GP.get_version()},"
        f" start at {start_time}, end at {end_time} \n"
        f"=======>{report_file} is saved. \n"
    )
    input("=======>press [Enter] key to Show logs.")


def rmdir(path):
    try:
        if sys.platform == "win32":
            if os.path.isfile(path) or os.path.islink(path):
                os.remove(path)
            else:
                shutil.rmtree(path)
        else:
            subprocess.call(f"rm -rf {path}".split())
    except OSError:
        print(f"Error: {path} : cannot remove")
        pass


def get_local_path(path):
    return os.path.join(GP.local_path, path)


def get_remote_path(path):
    return f"{GP.remote_path}/{path}"


def get_local_md5(local):
    md5_hash = hashlib.md5()
    with open(local, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            md5_hash.update(byte_block)
    return md5_hash.hexdigest()


def check_shell_any_device(cmd, pattern=None, fetch=False):
    print(f"\nexecuting command: {cmd}")
    if pattern: # check output valid
        print("pattern case")
        try:
            output = subprocess.check_output(cmd.split()).decode('utf-8')
        except UnicodeDecodeError:
            output = subprocess.check_output(cmd.split()).decode('gbk')
        res = pattern in output
        print(f"--> output: {output}")
        print(f"--> pattern [{pattern}] {'FOUND' if res else 'NOT FOUND'} in output")
        return res, output
    elif fetch:
        output = subprocess.check_output(cmd.split()).decode()
        print(f"--> output: {output}")
        return True, output
    else: # check cmd run successfully
        print("other case")
        return subprocess.check_call(cmd.split()) == 0, ""


def check_shell(cmd, pattern=None, fetch=False, head=None):
    if head is None:
        head = GP.hdc_head
    cmd = f"{head} {cmd}"
    print(f"\nexecuting command: {cmd}")
    if pattern: # check output valid
        output = subprocess.check_output(cmd.split()).decode()
        res = pattern in output
        print(f"--> output: {output}")
        print(f"--> pattern [{pattern}] {'FOUND' if res else 'NOT FOUND'} in output")
        return res
    elif fetch:
        output = subprocess.check_output(cmd.split()).decode()
        print(f"--> output: {output}")
        return output
    else: # check cmd run successfully
        return subprocess.check_call(cmd.split()) == 0


def get_shell_result(cmd, pattern=None, fetch=False):
    cmd = f"{GP.hdc_head} {cmd}"
    print(f"\nexecuting command: {cmd}")
    return subprocess.check_output(cmd.split()).decode()


def check_rate(cmd, expected_rate):
    send_result = get_shell_result(cmd)
    rate = float(send_result.split("rate:")[1].split("kB/s")[0])
    return rate > expected_rate


def check_dir(local, remote, is_single_dir=False):
    def _get_md5sum(remote, is_single_dir=False):
        if is_single_dir:
            cmd = f"{GP.hdc_head} shell md5sum {remote}/*"
        else:
            cmd = f'{GP.hdc_head} shell find {remote} -type f -exec md5sum {{}}'
        result = subprocess.check_output(cmd.split()).decode()
        return result
    
    def _calculate_md5(file_path):
        md5 = hashlib.md5()
        try:
            with open(file_path, 'rb') as f:
                for chunk in iter(lambda: f.read(4096), b""):
                    md5.update(chunk)
            return md5.hexdigest()
        except PermissionError:
            return "PermissionError"
        except FileNotFoundError:
            return "FileNotFoundError"
    print("remote:" + remote)
    output = _get_md5sum(remote)
    print(output)

    result = 1
    for line in output.splitlines():
        if len(line) < 32: # length of MD5
            continue
        expected_md5, file_name = line.split()[:2]
        if is_single_dir:
            file_name = file_name.replace(f"{remote}", "")
        elif GP.remote_path in remote:
            file_name = file_name.split(GP.remote_dir_path)[1].replace("/", "\\")
        else:
            file_name = file_name.split(remote)[1].replace("/", "\\")
        file_path = os.path.join(os.getcwd(), GP.local_path) + file_name  # 构建完整的文件路径
        if is_single_dir:
            file_path = os.path.join(os.getcwd(), local) + file_name
        print(file_path)
        actual_md5 = _calculate_md5(file_path)
        print(f"Expected: {expected_md5}")
        print(f"Actual: {actual_md5}")
        print(f"MD5 matched {file_name}")
        if actual_md5 != expected_md5:
            print(f"[Fail]MD5 mismatch for {file_name}")
            result *= 0

    return (result == 1)


def _check_file(local, remote, head=None):
    if head is None:
        head = GP.hdc_head
    if remote.startswith("/proc"):
        local_size = os.path.getsize(local)
        if local_size > 0:
            return True
        else:
            return False
    else:
        cmd = f"shell md5sum {remote}"
        local_md5 = get_local_md5(local)
        return check_shell(cmd, local_md5, head=head)


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
    if (args == "-s" and app.endswith(".hap")) or (args == "" and app.endswith(".hsp")):
        return check_shell(install_cmd, "failed to install bundle")
    else:
        return check_shell(install_cmd, "successfully") and _check_app_installed(bundle, "s" in args)


def check_app_uninstall(bundle, args=""):
    uninstall_cmd = f"uninstall {args} {bundle}"
    return check_shell(uninstall_cmd, "successfully") and not _check_app_installed(bundle, "s" in args)


def check_app_install_multi(tables, args=""):
    apps = []
    bundles = []
    for app, bundle in tables.items() :
        app = os.path.join(GP.local_path, app)
        apps.append(app)
        bundles.append(bundle)

    apps_str = " ".join(apps)
    install_cmd = f"install {args} {apps_str}"

    if ((args == "-s" and re.search(".hap", apps_str)) or (re.search(".hsp", apps_str) and re.search(".hap", apps_str))
        or (args == "" and 0 == apps_str.count(".hap"))):
        if not check_shell(install_cmd, "failed to install bundle"):
            return False
    else:
        if not check_shell(install_cmd, "successfully"):
            return False

        for bundle in bundles:
            if not _check_app_installed(bundle, "s" in args):
                return False

    return True


def check_app_uninstall_multi(tables, args=""):
    for app, bundle in tables.items() :
        if not check_app_uninstall(bundle, args):
            return False

    return True


def check_app_not_exist(app, bundle, args=""):
    app = os.path.join(GP.local_path, app)
    install_cmd = f"install {args} {app}"
    if (args == "-s" and app.endswith(".hap")) or (args == "" and app.endswith(".hsp")):
        return check_shell(install_cmd, "Error opening file")
    return False


def check_app_dir_not_exist(app, bundle, args=""):
    app = os.path.join(GP.local_path, app)
    install_cmd = f"install {args} {app}"
    return check_shell(install_cmd, "Not any installation package was found")


def check_hdc_cmd(cmd, pattern=None, head=None, is_single_dir=True, **args):
    if head is None:
        head = GP.hdc_head
    if cmd.startswith("file"):
        if not check_shell(cmd, "FileTransfer finish", head=head):
            return False
        if cmd.startswith("file send"):
            local, remote = cmd.split()[-2:]
            if remote[-1] == '/' or remote[-1] == '\\':
                remote = f"{remote}{os.path.basename(local)}"
        else:
            remote, local = cmd.split()[-2:]
            if local[-1] == '/' or local[-1] == '\\':
                local = f"{local}{os.path.basename(remote)}"
        if "-b" in cmd:
            mnt_debug_path = "mnt/debug/100/debug_hap/"
            remote = f"{mnt_debug_path}/{GP.debug_app}/{remote}"
        if os.path.isfile(local):
            return _check_file(local, remote, head=head)
        else:
            return check_dir(local, remote, is_single_dir=is_single_dir)
    elif cmd.startswith("install"):
        bundle = args.get("bundle", "invalid")
        opt = " ".join(cmd.split()[1:-1])
        return check_shell(cmd, "successfully") and _check_app_installed(bundle, "s" in opt)

    elif cmd.startswith("uninstall"):
        bundle = cmd.split()[-1]
        opt = " ".join(cmd.split()[1:-1])
        return check_shell(cmd, "successfully") and not _check_app_installed(bundle, "s" in opt)

    else:
        return check_shell(cmd, pattern, head=head, **args)


def check_soft_local(local_source, local_soft, remote):
    cmd = f"file send {local_soft} {remote}"
    if not check_shell(cmd, "FileTransfer finish"):
        return False
    return _check_file(local_source, remote)


def check_soft_remote(remote_source, remote_soft, local_recv):
    check_hdc_cmd(f"shell ln -s {remote_source} {remote_soft}")
    cmd = f"file recv {remote_soft} {local_recv}"
    if not check_shell(cmd, "FileTransfer finish"):
        return False
    return _check_file(local_recv, get_remote_path(remote_source))


def switch_usb():
    res = check_hdc_cmd("tmode usb")
    time.sleep(3)
    if res:
        GP.hdc_head = f"{GP.hdc_exe} -t {GP.device_name}"
    return res


def copy_file(src, dst):
    try:
        shutil.copy2(src, dst)
        print(f"File copied successfully from {src} to {dst}")
    except IOError as e:
        print(f"Unable to copy file. {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")


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


def gen_file(path, size):
    index = 0
    path = os.path.abspath(path)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT, 0o755)

    while index < size:
        os.write(fd, os.urandom(1024))
        index += 1024
    os.close(fd)


def gen_version_file(path):
    with open(path, "w") as f:
        f.write(GP.get_version())


def gen_zero_file(path, size):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT, 0o755)
    os.write(fd, b'0' * size)
    os.close(fd)


def create_file_with_size(path, size):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT, 0o755)
    os.write(fd, b'\0' * size)
    os.close(fd)


def gen_soft_link():
    print("generating soft link ...")
    depth_1_path = os.path.join(GP.local_path, "d1")
    depth_2_path = os.path.join(GP.local_path, "d1", "d2")
    if not os.path.exists(os.path.join(GP.local_path, "d1")):
        os.mkdir(depth_1_path)
        os.mkdir(depth_2_path)
        copy_file(os.path.join(GP.local_path, "small"), depth_2_path)
    try:
        os.symlink("small", os.path.join(GP.local_path, "soft_small"))
    except FileExistsError:
        print("soft_small already exists")
    except OSError:
        print("生成soft_small失败，需要使用管理员权限用户执行软链接生成")
    try:
        os.symlink("d1", os.path.join(GP.local_path, "soft_dir"))
    except FileExistsError:
        print("soft_dir already exists")
    except OSError:
        print("生成soft_dir失败，需要使用管理员权限用户执行软链接生成")


def gen_file_set():
    print("generating empty file ...")
    gen_file(os.path.join(GP.local_path, "empty"), 0)

    print("generating small file ...")
    gen_file(os.path.join(GP.local_path, "small"), 102400)

    print("generating medium file ...")
    gen_file(os.path.join(GP.local_path, "medium"), 200 * 1024 ** 2)

    print("generating large file ...")
    gen_file(os.path.join(GP.local_path, "large"), 2 * 1024 ** 3)

    print("generating text file ...")
    gen_zero_file(os.path.join(GP.local_path, "word_100M.txt"), 100 * 1024 ** 2)

    gen_soft_link()
    print("generating package dir ...")
    if not os.path.exists(os.path.join(GP.local_path, "package")):
        os.mkdir(os.path.join(GP.local_path, "package"))
    for i in range(1, 6):
        gen_file(os.path.join(GP.local_path, "package", f"fake.hap.{i}"), 20 * 1024 ** 2)

    print("generating deep dir ...")
    deepth = 4
    deep_path = os.path.join(GP.local_path, "deep_dir")
    if not os.path.exists(deep_path):
        os.mkdir(deep_path)
    for deep in range(deepth):
        deep_path = os.path.join(deep_path, f"deep_dir{deep}")
        if not os.path.exists(deep_path):
            os.mkdir(deep_path)
    gen_file(os.path.join(deep_path, "deep"), 102400)

    print("generating dir with file ...")
    dir_path = os.path.join(GP.local_path, "problem_dir")
    rmdir(dir_path)
    os.mkdir(dir_path)
    gen_file(os.path.join(dir_path, "small2"), 102400)

    fuzz_count = 47 # 47 is the count that circulated the file transfer
    data_unit = 1024 # 1024 is the size that circulated the file transfer
    data_extra = 936 # 936 is the size that cased the extra file transfer
    for i in range(fuzz_count):
        create_file_with_size(
            os.path.join(dir_path, f"file_{i * data_unit+data_extra}.dat"), i * data_unit + data_extra)
    print("generating empty dir ...")
    dir_path = os.path.join(GP.local_path, "empty_dir")
    rmdir(dir_path)
    os.mkdir(dir_path)
    print("generating version file ...")
    gen_version_file(os.path.join(GP.local_path, "version"))


def gen_package_dir():
    print("generating app dir ...")
    dir_path = os.path.join(GP.local_path, "app_dir")
    if os.path.exists(dir_path):
        rmdir(dir_path)
    os.mkdir(dir_path)
    app = os.path.join(GP.local_path, "AACommand07.hap")
    dst_dir = os.path.join(GP.local_path, "app_dir")
    if not os.path.exists(app):
        print(f"Source file {app} does not exist.")
    else:
        copy_file(app, dst_dir)


def prepare_source():
    version_file = os.path.join(GP.local_path, "version")
    if os.path.exists(version_file):
        with open(version_file, "r") as f:
            version = f.read()
            if version == GP.get_version():
                print(f"hdc test version is {GP.get_version()}, check ok, skip prepare.")
                return
    print(f"in prepare {GP.local_path},wait for 2 mins.")
    current_path = os.getcwd()

    if os.path.exists(GP.local_path):
        #打开local_path遍历其中的文件，删除hap hsp以外的所有文件
        for file in os.listdir(GP.local_path):
            if file.endswith(".hap") or file.endswith(".hsp"):
                continue
            file_path = os.path.join(GP.local_path, file)
            rmdir(file_path)
    else:
        os.mkdir(GP.local_path)
    
    gen_file_set()


def add_prepare_source():
    deep_path = os.path.join(GP.local_path, "deep_test_dir")
    print("generating deep test dir ...")
    absolute_path = os.path.abspath(__file__)
    deepth = (255 - 9 - len(absolute_path)) % 14
    os.mkdir(deep_path)
    for deep in range(deepth):
        deep_path = os.path.join(deep_path, f"deep_test_dir{deep}")
        os.mkdir(deep_path)
    gen_file(os.path.join(deep_path, "deep_test"), 102400)

    recv_dir = os.path.join(GP.local_path, "recv_test_dir")
    print("generating recv test dir ...")
    os.mkdir(recv_dir)


def update_source():
    deep_path = os.path.join(GP.local_path, "deep_test_dir")
    if not os.path.exists(deep_path):
        print("generating deep test dir ...")
        absolute_path = os.path.abspath(__file__)
        deepth = (255 - 9 - len(absolute_path)) % 14
        os.mkdir(deep_path)
        for deep in range(deepth):
            deep_path = os.path.join(deep_path, f"deep_test_dir{deep}")
            os.mkdir(deep_path)
        gen_file(os.path.join(deep_path, "deep_test"), 102400)

    recv_dir = os.path.join(GP.local_path, "recv_test_dir")
    if not os.path.exists(recv_dir):
        print("generating recv test dir ...")
        os.mkdir(recv_dir)


def load_testcase():
    if not GP.load_testcase:
        print("load testcase failed")
        return False
    print("load testcase success")
    return True


def check_library_installation(library_name):
    try:
        importlib.metadata.version(library_name)
        return 0
    except importlib.metadata.PackageNotFoundError:
        print(f"\n\n{library_name} is not installed.\n\n")
        print(f"try to use command below:")
        print(f"pip install {library_name}")
        return 1


def check_subprocess_cmd(cmd, process_num, timeout):

    for i in range(process_num):
        p = subprocess.Popen(cmd.split())
    try:
        p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()


def create_file_commands(local, remote, mode, num):
    if mode == "send":
        return [f"{GP.hdc_head} file send {local} {remote}_{i}" for i in range(num)]
    elif mode == "recv":
        return [f"{GP.hdc_head} file recv {remote}_{i} {local}_{i}" for i in range(num)]
    else:
        return []


def create_dir_commands(local, remote, mode, num):
    if mode == "send":
        return [f"{GP.hdc_head} file send {local} {remote}" for _ in range(num)]
    elif mode == "recv":
        return [f"{GP.hdc_head} file recv {remote} {local}" for _ in range(num)]
    else:
        return []


def execute_commands(commands):
    processes = [subprocess.Popen(cmd.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE) for cmd in commands]
    while processes:
        for p in processes:
            if not handle_process(p, processes, assert_out="FileTransfer finish"):
                return False
    return True


def handle_process(p, processes, assert_out="FileTransfer finish"):
    if p.poll() is not None:
        stdout, stderr = p.communicate(timeout=512)  # timeout wait 512s
        if stderr:
            print(f"{stderr.decode()}")
        if stdout:
            print(f"{stdout.decode()}")
        if assert_out is not None and stdout.decode().find(assert_out) == -1:
            return False
        processes.remove(p)
    return True


def check_files(local, remote, mode, num):
    res = 1
    for i in range(num):
        if mode == "send":
            if _check_file(local, f"{remote}_{i}"):
                res *= 1
            else:
                res *= 0
        elif mode == "recv":
            if _check_file(f"{local}_{i}", f"{remote}_{i}"):
                res *= 1
            else:
                res *= 0
    return res == 1


def check_dirs(local, remote, mode, num):
    res = 1
    for _ in range(num):
        if mode == "send":
            end_of_file_name = os.path.basename(local)
            if check_dir(local, f"{remote}/{end_of_file_name}", is_single_dir=True):
                res *= 1
            else:
                res *= 0
        elif mode == "recv":
            end_of_file_name = os.path.basename(remote)
            local = os.path.join(local, end_of_file_name)
            if check_dir(f"{local}", f"{remote}", is_single_dir=True):
                res *= 1
            else:
                res *= 0
    return res == 1


def make_multiprocess_file(local, remote, mode, num, task_type):
    if num < 1:
        return False

    if task_type == "file":
        commands = create_file_commands(local, remote, mode, num)
    elif task_type == "dir":
        commands = create_dir_commands(local, remote, mode, num)
    else:
        return False

    print(commands[0])
    if not execute_commands(commands):
        return False

    if task_type == "file":
        return check_files(local, remote, mode, num)
    elif task_type == "dir":
        return check_dirs(local, remote, mode, num)
    else:
        return False


def hdc_get_key(cmd):
    test_cmd = f"{GP.hdc_head} {cmd}"
    result = subprocess.check_output(test_cmd.split()).decode()
    return result


def check_hdc_version(cmd, version):

    def _convert_version_to_hex(_version):
        parts = _version.split("Ver: ")[1].split('.')
        hex_version = ''.join(parts)
        return int(hex_version, 36)
    
    expected_version = _convert_version_to_hex(version)
    cmd = f"{GP.hdc_head} {cmd}"
    print(f"\nexecuting command: {cmd}")
    if version is not None: # check output valid
        output = subprocess.check_output(cmd.split()).decode().replace("\r", "").replace("\n", "")
        real_version = _convert_version_to_hex(output)
        print(f"--> output: {output}")
        print(f"--> your local [{version}] is"
            f" {'' if expected_version <= real_version else 'too old to'} fit the version [{output}]"
        )
        return expected_version <= real_version


def check_cmd_time(cmd, pattern, duration, times):
    if times < 1:
        print("times should be bigger than 0.")
        return False
    if pattern is None:
        fetchable = True
    else:
        fetchable = False
    start_time = time.time() * 1000
    print(f"{cmd} start {start_time}")
    res = []
    for i in range(times):
        start_in = time.time() * 1000
        if pattern is None:
            subprocess.check_output(f"{GP.hdc_head} {cmd}".split())
        elif not check_shell(cmd, pattern, fetch=fetchable):
            return False
        start_out = time.time() * 1000
        res.append(start_out - start_in)

    # 计算最大值、最小值和中位数
    max_value = max(res)
    min_value = min(res)
    median_value = sorted(res)[len(res) // 2]

    print(f"{GP.hdc_head} {cmd}耗时最大值:{max_value}")
    print(f"{GP.hdc_head} {cmd}耗时最小值:{min_value}")
    print(f"{GP.hdc_head} {cmd}耗时中位数:{median_value}")
    
    end_time = time.time() * 1000
    
    try: 
        timecost = int(end_time - start_time) / times
        print(f"{GP.hdc_head} {cmd}耗时平均值 {timecost}")
    except ZeroDivisionError:
        print(f"除数为0")
    
    if duration is None:
        duration = 150 * 1.2
    # 150ms is baseline timecost for hdc shell xxx cmd, 20% can be upper maybe system status
    return timecost < duration


def check_rom(baseline):

    def _try_get_size(message):
        try:
            size = int(message.split('\t')[0])
        except ValueError:
            size = -9999 * 1024 # error size
            print(f"try get size value error, from {message}")
        return size

    if baseline is None:
        baseline = 2200
    # 2200KB is the baseline of hdcd and libhdc.dylib.so size all together
    cmd_hdcd = f"{GP.hdc_head} shell du system/bin/hdcd"
    result_hdcd = subprocess.check_output(cmd_hdcd.split()).decode()
    hdcd_size = _try_get_size(result_hdcd)
    cmd_libhdc = f"{GP.hdc_head} shell du system/lib/libhdc.dylib.so"
    result_libhdc = subprocess.check_output(cmd_libhdc.split()).decode()
    if "directory" in result_libhdc:
        cmd_libhdc64 = f"{GP.hdc_head} shell du system/lib64/libhdc.dylib.so"
        result_libhdc64 = subprocess.check_output(cmd_libhdc64.split()).decode()
        if "directory" in result_libhdc64:
            libhdc_size = 0
        else:
            libhdc_size = _try_get_size(result_libhdc64)
    else:
        libhdc_size = _try_get_size(result_libhdc)
    all_size = hdcd_size + libhdc_size
    GP.hdcd_rom = all_size
    if all_size < 0:
        GP.hdcd_rom = "error"
        return False
    else:
        GP.hdcd_rom = f"{all_size} KB"
    if all_size > baseline:
        print(f"rom size is {all_size}, overlimit baseline {baseline}")
        return False
    else:
        print(f"rom size is {all_size}, underlimit baseline {baseline}")
        return True


def run_command_with_timeout(command, timeout):
    try:
        result = subprocess.run(command.split(), check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
        return result.stdout.decode(), result.stderr.decode()
    except subprocess.TimeoutExpired:
        return "", "Command timed out"
    except subprocess.CalledProcessError as e:
        return "", e.stderr.decode()


def check_cmd_block(command, pattern, timeout=600):
    # 启动子进程
    process = subprocess.Popen(command.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    # 用于存储子进程的输出
    output = ""

    try:
        # 读取子进程的输出
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.kill()
        output, _ = process.communicate(timeout=timeout)

    print(f"--> output: {output}")
    if pattern in output:
        return True
    else:
        return False


def check_version(version):
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            if not check_hdc_version("version", version) or not check_hdc_version("shell hdcd -v", version):
                print("version does not match, ignore this case")
                pytest.skip("Version does not match, test skipped.")
            return func(*args, **kwargs)
        return wrapper
    return decorator


@pytest.fixture(scope='class', autouse=True)
def load_gp(request):
    GP.load()


class ConfigFileNotFoundException(Exception):
    """配置文件未找到异常"""
    pass


def get_hdcd_pss():
    mem_string = get_shell_result("shell hidumper --mem `pidof hdcd`")
    print(f"--> hdcd mem: \n{mem_string}")
    pss_string = get_shell_result("shell hidumper --mem `pidof hdcd` | grep Total")
    if "Total" in pss_string:
        pss_value = int(re.sub(r"\s+", " ", pss_string.split('\n')[1]).split(' ')[2])
    else:
        print("error: can't get pss value, message:%s", pss_string)
        pss_value = 0
    return pss_value


def get_hdcd_fd_count():
    sep = '/'
    fd_string = get_shell_result(f"shell ls {sep}proc/`pidof hdcd`/fd | wc -l")
    print(f"--> hdcd fd cound: {fd_string}")
    try:
        end_symbol = get_end_symbol()
        fd_value = int(fd_string.split(end_symbol)[0])
    except ValueError:
        fd_value = 0
    return fd_value


def get_end_symbol():
    return sys.platform == 'win32' and "\r\n" or '\n'


def get_server_pid_from_file():
    is_ohos = "Harmony" in platform.system()
    if not is_ohos:
        tmp_dir_path = tempfile.gettempdir()
    else:
        tmp_dir_path = os.path.expanduser("~")
    pid_file = os.path.join(tmp_dir_path, ".HDCServer.pid")
    with open(pid_file, "r") as f:
        pid = f.read()
    try:
        pid = int(pid)
    except ValueError:
        pid = 0
    print(f"--> pid of hdcserver is {pid}")
    return pid


def check_unsupport_systems(systems):
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            cur_sys = platform.system()
            for system in systems:
                if system in cur_sys:
                    print("system not support, ignore this case")
                    pytest.skip("System not support, test skipped.")
            return func(*args, **kwargs)
        return wrapper
    return decorator


@check_unsupport_systems(["Harmony"])
def get_cmd_block_output(command, timeout=600):
    # 启动子进程
    process = subprocess.Popen(command.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    # 用于存储子进程的输出
    output = ""

    try:
        # 读取子进程的输出
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.kill()
        output, _ = process.communicate(timeout=timeout)

    print(f"--> output: {output}")
    return output


def get_cmd_block_output_and_error(command, timeout=600):
    print(f"cmd: {command}")
    # 启动子进程
    process = subprocess.Popen(command.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    # 用于存储子进程的输出
    output = ""
    error = ""

    try:
        # 读取子进程的输出
        output, error = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.kill()
        output, error = process.communicate(timeout=timeout)

    print(f"--> output: {output}")
    print(f"--> error: {error}")
    return output, error


def send_file(conn, file_name):
    """
    socket收发数据相关的功能函数，用于socket收发数据测试
    文件发送端发送文件到接收端
    1. Sender send file size(bytes string) to receiver
    2. The sender waits for the receiver to send back the file size
    3. The sender cyclically sends file data to the receiver
    """
    logger.info(f"send_file enter:{file_name}")
    file_path = os.path.join(GP.local_path, file_name)
    logger.info(f"send_file file full name:{file_path}")
    file_size = os.path.getsize(file_path)
    logger.info(f"send_file file size:{file_size}")

    # send size
    conn.send(str(file_size).encode('utf-8'))

    # recv file size for check
    logger.info(f"send_file: start recv check size")
    size_raw = conn.recv(1024)
    logger.info(f"send_file: check size_raw {size_raw}")
    if len(size_raw) == 0:
        logger.error(f"send_file: recv check size len is 0, exit")
        return
    file_size_recv = int(size_raw.decode('utf-8'))
    if file_size_recv != file_size:
        logger.error(f"send_file: check size failed, file_size_recv:{file_size_recv} file size:{file_size}")
        return

    logger.info(f"send_file start send file data")
    index = 0
    with open(file_path, 'rb') as f:
        while True:
            one_block = f.read(4096)
            if not one_block:
                logger.info(f"send_file index:{index} read 0 block")
                break
            conn.send(one_block)
            index = index + 1


def process_conn(conn, addr):
    """
    socket收发数据相关的功能函数，用于socket收发数据测试
    Server client interaction description:
    1. client send "get [file_name]" to server
    2. server send file size(string) to client
    3. client send back size to server
    4. server send file data to client
    """
    conn.settimeout(5)  # timeout 5 second
    try:
        logger.info(f"server accept, addr:{str(addr)}")
        message = conn.recv(1024)
        message_str = message.decode('utf-8')
        logger.info(f"conn recv msg [{len(message_str)}] {message_str}")
        if len(message) == 0:
            conn.close()
            logger.info(f"conn msg len is 0, close {conn} addr:{addr}")
            return
        cmds = message_str.split()
        logger.info(f"conn cmds:{cmds}")
        cmd = cmds[0]
        if cmd == "get":  # ['get', 'xxxx']
            logger.info(f"conn cmd is get, file name:{cmds[1]}")
            file_name = cmds[1]
            send_file(conn, file_name)
        logger.info(f"conn normal close")
    except socket.timeout:
        logger.info(f"conn:{conn} comm timeout, addr:{addr}")
    except ConnectionResetError:
        logger.info(f"conn:{conn} ConnectionResetError, addr:{addr}")
    conn.close()


def server_loop(port_num):
    """
    socket收发数据相关的功能函数，用于socket收发数据测试
    服务端，每次监听，接入连接，收发数据结束后关闭监听
    """
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(('localhost', port_num))
    server_socket.listen()
    logger.info(f"server start listen {port_num}")
    server_socket.settimeout(10)  # timeout 10 second

    try:
        conn, addr = server_socket.accept()
        process_conn(conn, addr)
    except socket.timeout:
        logger.error(f"server accept timeout, port:{port_num}")

    server_socket.close()
    logger.info(f"server exit, port:{port_num}")


def recv_file_data(client_socket, file_path, file_size):
    """
    socket收发数据相关的功能函数，用于socket收发数据测试
    """
    logger.info(f"client: start recv file data, file size:{file_size}, file path:{file_path}")
    with open(file_path, 'wb') as f:
        recv_size = 0
        while recv_size < file_size:
            one_block = client_socket.recv(4096)
            if not one_block:
                logger.info(f"client: read block size 0, exit")
                break
            f.write(one_block)
            recv_size += len(one_block)
    logger.info(f"client: recv file data finished, recv size:{recv_size}")


def client_get_file(port_num, file_name, file_save_name):
    """
    socket收发数据相关的功能函数，用于socket收发数据测试
    """
    logger.info(f"client: connect port:{port_num}")
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.settimeout(10)  # timeout 10 second
    try:
        client_socket.connect(('localhost', port_num))
    except socket.timeout:
        logger.info(f"client connect timeout, port:{port_num}")
        return

    try:
        cmd = f"get {file_name}"
        logger.info(f"client: send cmd:{cmd}")
        client_socket.send(cmd.encode('utf-8'))

        # recv file size
        size_raw = client_socket.recv(1024)
        logger.info(f"client: recv size_raw {size_raw}")
        if len(size_raw) == 0:
            logger.info(f"client: cmd:{cmd} recv size_raw len is 0, exit")
            return
        file_size = int(size_raw.decode('utf-8'))
        logger.info(f"client: file size {file_size}")

        file_path = os.path.join(GP.local_path, file_save_name)
        if os.path.exists(file_path):
            logger.info(f"client: file {file_path} exist, delete")
            try:
                os.remove(file_path)
            except OSError as error:
                logger.info(f"delete {file_path} failed: {error.strerror}")

        # send size msg to client for check
        logger.info(f"client: Send back file size:{size_raw}")
        client_socket.send(size_raw)
        recv_file_data(client_socket, file_path, file_size)
    except socket.timeout:
        logger.error(f"client communication timeout, port:{port_num}")
        return
    finally:
        logger.info("client socket close")
        client_socket.close()
    logger.info("client exit")

def get_bundle_info(bundle_name, key, is_shared=False):
    dump = "dump-shared" if is_shared else "dump"
    cmd = f"hdc shell bm {dump} -n {bundle_name}"
    """
    cmd output example:
    com.example.xxx:
    {
        "k1": v1,
        "k2": v2,
        ...
    }
    """
    try:
        output = subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT)
        result = output.decode("utf-8")
        if "error" in result:
            return ""
        # clear first unused content
        infos = json.loads("".join((result.split("\n")[1:])))
        return infos if key == "" else infos[key]
    except subprocess.CalledProcessError as e:
        logger.error(f"bm dump failed: {e.stderr.decode()}")
        return ""


def get_md5sum_local(file_name):
    if sys.platform == "win32":
        cmd = "certutil -hashfile " + file_name + " MD5"
        output = subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT)
        result = output.decode(encoding="gbk", errors="ignore")
        return str(result).split("\r\n")[1]
    else:
        cmd = "md5sum " + file_name
        output = subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT)
        result = output.decode(encoding="gbk", errors="ignore")
        return str(result).split(" ")[0]


def generate_random_string(length=128):
    # 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
    characters = string.ascii_letters
    return ''.join(random.choices(characters, k=length))