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

import csv
import hashlib
import json
import logging
import os
import random
import re
import stat
import shutil
import subprocess
import sys
import threading
import time
from multiprocessing import Process

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
        logging.basicConfig(level=logging.INFO,
                            format='%(asctime)s %(filename)s [line:%(lineno)d] %(levelname)s %(message)s',
                            datefmt='%d %b %Y %H:%M:%S',
            )
        logging.basicConfig(level=logging.WARNING,
                            format='%(asctime)s %(filename)s [line:%(lineno)d] %(levelname)s %(message)s',
                            datefmt='%d %b %Y %H:%M:%S',
                            ) 

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
        version = f"v1.0.2a"
        return version


def pytest_run(args):
    file_list = []
    file_list.append("entry-default-signed-debug.hap")
    file_list.append("libA_v10001.hsp")
    file_list.append("libB_v10001.hsp")
    for file in file_list:
        if not os.path.exists(os.path.join(GP.local_path, file)):
            print(f"No {file} File!")
            print("请将package.zip中的安装包文件解压到当前脚本resource目录中,操作完成该步骤后重新执行脚本。")
            print("Please unzip package.zip to resource directory, please rerun after operation.")
            input("[ENTER]")
            return
    start_time = time.strftime('%Y-%m-%d %H:%M:%S',time.localtime(time.time()))
    if args.count is not None:
        for i in range(args.count):
            print(f"------------The {i}/{args.count} Test-------------")
            timestamp = time.time()
            pytest_args = [
                '--verbose', args.verbose,
                '--report=report.html',
                '--title=HDC Test Report 'f"{GP.get_version()}",
                '--tester=tester001',
                '--template=1',
                '--desc='f"{args.verbose}:{args.desc}"
            ]
            pytest.main(pytest_args)
    end_time = time.strftime('%Y-%m-%d %H:%M:%S',time.localtime(time.time()))
    report_time = time.strftime('%Y-%m-%d_%H_%M_%S',time.localtime(time.time()))
    report_dir = os.path.join(os.getcwd(), "reports")
    report_file = os.path.join(report_dir, f"{report_time}report.html")
    print(f"Test over, the script version is {GP.get_version()},"
        f" start at {start_time}, end at {end_time} \n"
        f"=======>{report_file} is saved. \n"
    )
    input("=======>press [Enter] key to Show logs.")


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
        print(f"--> output: {output}")
        print(f"--> pattern [{pattern}] {'FOUND' if res else 'NOT FOUND'} in output")
        return res
    elif fetch:
        output = subprocess.check_output(cmd.split()).decode()
        print(f"--> output: {output}")
        return output
    else: # check cmd run successfully
        return subprocess.check_call(cmd.split()) == 0


def _check_dir(local, remote):
    def _get_md5sum(remote):
        cmd = f"{GP.hdc_head} shell md5sum {remote}/*"
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
    print("remote" + remote)
    output = _get_md5sum(remote)
    print(output)
    for line in output.splitlines():
        if len(line) < 32: # length of MD5
            continue
        expected_md5, file_name = line.split()[:2]
        file_name = file_name.replace(f"{remote}/", "")
        file_path = os.path.join(os.getcwd(), local, file_name)  # 构建完整的文件路径
        print(file_path)
        actual_md5 = _calculate_md5(file_path)
        logging.info(f"Expected: {expected_md5}")
        logging.info(f"Actual: {actual_md5}")
        logging.info(f"MD5 matched {file_name}")
        if actual_md5 != expected_md5:
            logging.info(f"[Fail]MD5 mismatch for {file_name}")
            return False
    return True
        

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


def check_app_install_multi(tables, args=""):
    apps = []
    bundles = []
    for app, bundle in tables.items() :
        app = os.path.join(GP.local_path, app)
        apps.append(app)
        bundles.append(bundle)

    apps_str = " ".join(apps)
    install_cmd = f"install {args} {apps_str}"

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

    print(f"in prepare {GP.local_path},wait for 2 mins.")
    current_path = os.getcwd()
    os.mkdir(GP.local_path)
    print("generating empty file ...")
    gen_file(os.path.join(GP.local_path, "empty"), 0)

    print("generating small file ...")
    gen_file(os.path.join(GP.local_path, "small"), 102400)

    print("generating medium file ...")
    gen_file(os.path.join(GP.local_path, "medium"), 200 * 1024 ** 2)

    print("generating large file ...")
    gen_file(os.path.join(GP.local_path, "large"), 2 * 1024 ** 3)

    print("generating soft link ...")
    os.symlink("small", os.path.join(GP.local_path, "soft_small"))

    print("generating package dir ...")
    os.mkdir(os.path.join(GP.local_path, "package"))
    for i in range(1, 6):
        gen_file(os.path.join(GP.local_path, "package", f"fake.hap.{i}"), 20 * 1024 ** 2)

    print("generating deep dir ...")
    deepth = 4
    deep_path = os.path.join(GP.local_path, "deep_dir")
    os.mkdir(deep_path)
    for deep in range(deepth):
        deep_path = os.path.join(deep_path, f"deep_dir{deep}")
        os.mkdir(deep_path)
    gen_file(os.path.join(deep_path, "deep"), 102400)

    print("generating dir with small file ...")
    dir_path = os.path.join(GP.local_path, "normal_dir")
    rmdir(dir_path)
    os.mkdir(dir_path)
    gen_file(os.path.join(dir_path, "small2"), 102400)

    print("generating empty dir ...")
    dir_path = os.path.join(GP.local_path, "empty_dir")
    rmdir(dir_path)
    os.mkdir(dir_path)


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


def check_subprocess_cmd(cmd, process_num, timeout):

    for i in range(process_num):
        p = subprocess.Popen(cmd.split())
    try:
        p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()


def check_process_mixed(process_num, timeout, local, remote):
    multi_num = process_num
    list_send = []
    list_recv = []
    sizes = {"small", "medium", "empty"}
    for i in range(multi_num):
        for size in sizes:
            cmd_send = f"file send {get_local_path(f'{size}')} {get_remote_path(f'it_{size}_mix_{i}')}"
            cmd_recv = f"file recv {get_remote_path(f'it_{size}_mix_{i}')} {get_local_path(f'recv_{size}_mix_{i}')}"
            list_send.append(Process(target=check_hdc_cmd, args=(cmd_send, )))
            list_recv.append(Process(target=check_hdc_cmd, args=(cmd_recv, )))
            logging.info(f"RESULT:{cmd_send}")  # 打印命令的输出  
    for send in list_send:
        wait_time = random.uniform(0, 1)
        send.start()
        time.sleep(wait_time)
    for send in list_send:
        send.join()

    for recv in list_recv:
        wait_time = random.uniform(0, 1)
        recv.start()
        time.sleep(wait_time)
    for recv in list_recv:
        recv.join()
        wait_time = random.uniform(0, 1)
        time.sleep(wait_time)


def execute_lines_in_file(file_path):
    if not os.path.exists(file_path):
        flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
        modes = stat.S_IWUSR | stat.S_IRUSR
        with os.fdopen(os.open(file_path, flags, modes), 'w') as file:
            file.write("1,hdc shell ls")
    with open(file_path, 'r') as file:
        lines = file.readlines()  
        for line in lines:
            test_time = line.split(',')[0]
            test_cmd = line.split(',')[1]
            pattern = r"^hdc"
            match = re.search(pattern, test_cmd)
            if match:
                result = test_cmd.replace(match.group(0), "").lstrip()
                test_cmd = f"{GP.hdc_head} {result}"
            
            for i in range(int(test_time)):
                logging.info(f"THE {i+1}/{test_time} TEST,COMMAND IS:{test_cmd}")
                output = subprocess.check_output(test_cmd.split()).decode()
                logging.info(f"RESULT:{output}")  # 打印命令的输出 


def make_multiprocess_file(local, remote, mode, num, task_type):
    if num < 1:
        return False
    if task_type == "file":
        if mode == "send" :
            file_list = [f"{GP.hdc_head} file send {local} {remote}_{i}" for i in range(num)]
        elif mode == "recv":
            file_list = [f"{GP.hdc_head} file recv {remote}_{i} {local}_{i}" for i in range(num)]
        else:
            return False
    if task_type == "dir":
        if mode == "send" :
            file_list = [f"{GP.hdc_head} file send {local} {remote}" for _ in range(num)]
        elif mode == "recv":
            file_list = [f"{GP.hdc_head} file recv {remote} {local}" for _ in range(num)]
        else:
            return False        
    print(file_list[0])
    p_list = [subprocess.Popen(cmd.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE) for cmd in file_list]
    logging.info(f"{mode} target {num} start")
    while(len(p_list)):
        for p in p_list:
            if p.poll() is not None:
                stdout, stderr = p.communicate(timeout=512) # timeout wait 512s
                if stderr:
                    logging.error(f"{stderr.decode()}")
                if stdout:
                    logging.info(f"{stdout.decode()}")
                if stdout.decode().find("FileTransfer finish") == -1:
                    return False
                p_list.remove(p)
    res = 1
    if task_type == "file":
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
    if task_type == "dir":
        for _ in range(num):
            if mode == "send":
                end_of_file_name = os.path.basename(local)
                if _check_dir(local, f"{remote}/{end_of_file_name}"):
                    res *= 1
                else:
                    res *= 0
            elif mode == "recv":
                end_of_file_name = os.path.basename(remote)
                local = os.path.join(local, end_of_file_name)
                if _check_dir(f"{local}", f"{remote}"):
                    res *= 1
                else:
                    res *= 0
    return res == 1


def hdc_get_key(cmd):
    test_cmd = f"{GP.hdc_head} {cmd}"
    result = subprocess.check_output(test_cmd.split()).decode()
    return result


def start_subprocess_cmd(cmd, num, assert_out):
    if num < 1:
        return False
    cmd_list = [f"{GP.hdc_head} {cmd}" for _ in range(num)]
    p_list = [subprocess.Popen(cmd.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE) for cmd in cmd_list]
    logging.info(f"{cmd} target {num} start")
    while(len(p_list)):
        for p in p_list:
            if p.poll() is not None:
                stdout, stderr = p.communicate(timeout=512)
                if stderr:
                    logging.error(f"{stderr.decode()}")
                if stdout:
                    logging.info(f"{stdout.decode()}")
                if assert_out is not None and stdout.decode().find(assert_out) == -1:
                    return False
                p_list.remove(p)
    return True


def check_hdc_version(cmd, version):

    def _convert_version_to_hex(_version):
        parts = _version.split("Ver: ")[1].split('.')
        hex_version = ''.join(parts)
        return int(hex_version, 16)
    
    expected_version = _convert_version_to_hex(version)
    cmd = f"{GP.hdc_head} -v"
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
    if pattern == None:
        fetchable = True
    else:
        fetchable = False
    start_time = time.time() * 1000
    print(f"{cmd} start {start_time}")
    res = []
    for i in range(times):
        start_in = time.time() * 1000
        check_shell(cmd, pattern, fetch = fetchable)
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
    
    timecost = int(end_time - start_time) / times
    print(f"{GP.hdc_head} {cmd}耗时平均值 {timecost}")
    if duration is None:
        duration = 150 * 1.2
    # 150ms is baseline timecost for hdc shell xxx cmd, 20% can be upper maybe system status
    return timecost < duration
