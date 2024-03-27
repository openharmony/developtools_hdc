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
# 1.运行环境
#   pycharm
#   Python 3.10
#   测试框架 pytest (pycharm setting -> Tools -> Testing -> pytest)
# 2.配置测试脚本
#   需配置hdc环境变量-NORMAL_HEAD为可执行程序hdc
#   配置TEST_FILE_PATH为测试用文件所在路径,取resource测试文件放入该路径下
#   测试tcp模式连接设备，需设备，PC连同一网络(手机热点)：配置TCP_CFG['ip']为设备的ip
import os
import random
import subprocess
import socket
import time
import threading

TCP_CFG = {
    'ip': '',
    'port': "8710",
}

NORMAL_HEAD = "hdc "
REMOTE_PATH = "/data/local/tmp/"
IP = ""
TEST_FILE_PATH = "{}{}".format(os.path.abspath("D:/Download/sdk/"), '\\')

HAP_ONE = {
    'HAP_NAME': "entry-default-signed-debug.hap",
    'PACKAGE_NAME': "com.hmos.diagnosis",
}

TCP_CONN = {
    'bright_screen': "shell \"power-shell setmode 602\"",
    'tmode': "{}{}".format("tmode port ", TCP_CFG['port']),
    'tconn': "{}{}{}{}".format("tconn ", TCP_CFG['ip'], ":", TCP_CFG['port']),
}

PATH = {
    'file_send': {
        'local': "{}{}".format(TEST_FILE_PATH, "hdc.log"),
        'remote': "{}{}".format(REMOTE_PATH, "hdc.log")
    },
    'dir_send': {
        'local': "{}{}".format(TEST_FILE_PATH, "log"),
        'remote': "{}{}".format(REMOTE_PATH, "log")
    },
    'file_recv': {
        'remote': "{}{}".format(REMOTE_PATH, "hdc.log"),
        'local': "{}{}".format(TEST_FILE_PATH, "dev_data")
    },
    'dir_recv': {
        'remote': "{}{}".format(REMOTE_PATH, "log"),
        'local': "{}{}".format(TEST_FILE_PATH, "hdc\\log")
    },
    'file_empty': {
        'local': "{}{}".format(TEST_FILE_PATH, "empty.txt"),
        'remote': "{}{}".format(REMOTE_PATH, "empty.txt")
    }
}

EXTRA_COMMANDS = {
    'global': ["kill -r", "kill", "-l5 start", "start -r", "-v", "version", "checkserver"],
    'smode': ["smode -r", "smode"],
    'boot': ["target boot"],
    'choose': "-t "
}

BASIC_COMMANDS = {
    'shell': [
        "shell \"ls\""
    ],
    'component': [
        "list targets", "list targets -v", "target mount"
    ],
    'file_task': [
        "{}{}{}{}".format("file send ", PATH['file_send']['local'], " ", PATH['file_send']['remote']),
        "{}{}{}{}".format("file send ", PATH['file_empty']['local'], " ", PATH['file_empty']['remote']),
        "{}{}{}{}".format("file send ", PATH['dir_send']['local'], " ", PATH['dir_send']['remote']),
        "{}{}{}{}".format("file recv ", PATH['file_recv']['remote'], " ", PATH['file_recv']['local']),
        "{}{}{}{}".format("file recv ", PATH['file_empty']['remote'], " ", PATH['file_empty']['local']),
        "{}{}{}{}".format("file recv ", PATH['dir_recv']['remote'], " ", PATH['dir_recv']['local'])
    ],
    'fport_task': [
        "fport tcp:1234 tcp:1080",
        "rport tcp:13608 localabstract:8888BananaBanana",
        "fport ls",
        "fport rm tcp:1234 tcp:1080"
    ],
    'install_task': [
        "{}{}{}".format("install ", TEST_FILE_PATH, HAP_ONE['HAP_NAME']),
        "{}{}".format("uninstall ", HAP_ONE['PACKAGE_NAME'])
    ]
}

TEST_FILES = {
    'one': {
        'send_file': "{}{}".format(TEST_FILE_PATH, "100M.txt"),
        'send_file_one': "{}{}".format(REMOTE_PATH, "a100M.txt"),
        'send_file_two': "{}{}".format(REMOTE_PATH, "c100M.txt"),
        'recv_file': "{}{}".format(REMOTE_PATH, "recv100M.txt"),
        'recv_file_one': "{}{}".format(TEST_FILE_PATH, "recv100M.txt"),
        'recv_file_two': "{}{}".format(TEST_FILE_PATH, "recv200M.txt"),
    },
    'two': {
        'send_file': "{}{}".format(TEST_FILE_PATH, "hdc_file.log"),
        'send_file_one': "{}{}".format(REMOTE_PATH, "send_one.log"),
        'send_file_two': "{}{}".format(REMOTE_PATH, "send_two.log"),
        'recv_file': "{}{}".format(REMOTE_PATH, "hdcd.log"),
        'recv_file_one': "{}{}".format(TEST_FILE_PATH, "recv_one.log"),
        'recv_file_two': "{}{}".format(TEST_FILE_PATH, "recv_two.log"),
    }
}

def command_judge(cmd):
    ret = False
    cmd_parts = cmd.split()
    if 'file send' in cmd and cmd[:9] == 'file send' and len(cmd_parts) == 4:
        ret = True
    if 'file recv' in cmd and cmd[:9] == 'file recv' and len(cmd_parts) == 4:
        ret = True
    if 'install' in cmd and cmd[:7] == 'install' and len(cmd_parts) == 2:
        ret = True
    return ret


def command_callback(cmd, head, need_del, res=""):
    cmd_parts = cmd.split()
    if 'file send' in cmd and cmd[:9] == 'file send' and len(cmd_parts) == 4:
        if need_del:
            assert "FileTransfer finish" in res
        check_file_send(cmd_parts[2], cmd_parts[3], head, need_del)
    if 'file recv' in cmd and cmd[:9] == 'file recv' and len(cmd_parts) == 4:
        if need_del:
            assert "FileTransfer finish" in res
        check_file_recv(cmd_parts[2], cmd_parts[3], head, need_del)
    if 'install' in cmd and cmd[:7] == 'install' and len(cmd_parts) == 2:
        check_install(head, res)
    if cmd == 'smode':
        time.sleep(4)
        check_root(head)
    if cmd == 'smode -r':
        time.sleep(4)
        check_user(head)
    if cmd == 'target boot':
        time.sleep(35)


def check_file_send(local_file, remote_file, head, need_del):
    ret = get_win_file_type(local_file)
    file_type = ""
    if 'file' in ret:
        file_type = '-f'
    if "dir" in ret:
        file_type = '-d'
    res = run_command_stdout("{}{}{}{}{}".format("shell \"[ ", file_type, " ", remote_file,
                             " ] && echo yes || echo no\""), head)
    assert 'yes' in res
    if file_type == '-d' or 'empty.txt' in local_file:
        rm_send_file(remote_file, head, need_del)
        return
    local_md5 = get_win_md5(local_file)
    remote_md5 = get_md5(remote_file, head)
    rm_send_file(remote_file, head, need_del)
    assert local_md5 == remote_md5
    print("check_file_send success ", res)


def rm_send_file(file_path, head, need_del):
    if need_del:
        run_command("{}{}{}{}".format("shell \"rm -rf ", file_path, "\"", head))


def check_file_recv(remote_file, local_file, head, need_del):
    if 'dir' in get_win_file_type(local_file) or 'empty.txt' in local_file:
        rm_recv_file(local_file, need_del)
        return
    res = run_command_stdout("{}{}{}".format("attrib ", local_file, ""))
    local_md5 = get_win_md5(local_file)
    remote_md5 = get_md5(remote_file, head)
    assert '-' not in res
    if local_md5 != remote_md5:
        print("check_file_recv fail ", remote_file, local_file)
    assert local_md5 == remote_md5
    rm_recv_file(local_file, need_del)
    print("check_file_recv success ", res)


def get_win_file_type(file_path):
    ret = run_command_stdout("{}{}{}".format("if exist ", file_path, " echo yes"), '')
    assert "yes" in ret
    res = run_command_stdout("{}{}{}".format("dir/ad ", file_path, " >nul 2>nul && echo dir || echo file"), '')
    return res


def rm_recv_file(file_path, need_del):
    if need_del:
        res = get_win_file_type(file_path)
        if "dir" in res:
            run_command("{}{}".format("rmdir /s/q ", file_path), "")
        if "file" in res:
            run_command("{}{}".format("del ", file_path), "")


def check_install(head, res):
    print("check_install")
    print(res)
    if "msg:install bundle successfully." not in res:
        print("install msg error")
        assert False
    res = run_command_stdout("shell \"bm dump -a\"", head)
    if HAP_ONE['PACKAGE_NAME'] in res:
        print("check_install success ", HAP_ONE['PACKAGE_NAME'])
        assert True
    else:
        assert False


def check_root(head):
    res = run_command_stdout("shell \"whoami\"", head)
    print("check_root res: ", res)
    assert 'root' in res


def check_user(head):
    res = run_command_stdout("shell \"whoami\"", head)
    print("check_user res: ", res)
    assert 'shell' in res
    return 'shell' in res


def get_devs(head=NORMAL_HEAD):
    res = run_command_stdout(BASIC_COMMANDS['component'][0], head)
    devs = res.split()
    print(devs)
    return devs


def tmode_to_tcp():
    run_command(TCP_CONN['tmode'])
    res = run_command_stdout(TCP_CONN['tconn'])
    print(res)
    if "Connect OK" in res:
        return True
    return False


def remote_server_start(server_head):
    global IP
    cmd = "{}{}".format(server_head, "-m")
    print(cmd)
    os.popen(cmd)


def run_command(cmd, head=NORMAL_HEAD, need_del=True, need_callback=True):
    command = "{}{}".format(head, cmd)
    if head != '':
        print(command)

    subprocess.Popen(command,
                     shell=True).communicate()
    if need_callback:
        command_callback(cmd, head, need_del)


def run_command_stdout(cmd, head=NORMAL_HEAD, need_del=True, need_callback=True):
    command = "{}{}".format(head, cmd)
    if head != '' and 'echo' not in cmd:
        print(command)
    dec = "UTF-8"
    if head == '':
        dec = 'gbk'
    res = subprocess.Popen(command,
                           shell=True,
                           stdout=subprocess.PIPE).communicate()
    res = res[0].decode(dec)
    if need_callback:
        command_callback(cmd, head, need_del, res)
    return res


def run_commands(cmds, head=NORMAL_HEAD, need_del=True):
    for command in cmds:
        run_command(command, head, need_del)


def get_basic_commands():
    commands = []
    for tasks in BASIC_COMMANDS.values():
        commands += tasks
    return commands


def run_global_cmd():
    global_commands = EXTRA_COMMANDS['global']
    run_commands(global_commands)


def run_split_commands(commands, head=NORMAL_HEAD):
    for command in commands:
        if command_judge(command):
            run_command_stdout(command, head, False)
        else:
            run_command(command, head, False)


def run_device_cmd(head=NORMAL_HEAD):
    run_command_stdout("{}{}{}{}".format("file send ", PATH['dir_send']['local'], " ", PATH['dir_recv']['remote']),
                       head, False)
    for smd in EXTRA_COMMANDS['smode']:
        run_command(smd)
        commands = get_basic_commands() + EXTRA_COMMANDS['boot']
        if smd == "smode -r":
            for item in commands:
                if "{}{}".format(REMOTE_PATH, "log") in item:
                    print(item)
                    commands.remove(item)
        run_split_commands(commands, head)


def extract_ip():
    global IP
    st = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        st.connect(('10.255.255.255', 1))
        IP = st.getsockname()[0]
    except Exception:
        IP = ""
    finally:
        st.close()
    return IP


def mix_path(path, i):
    ret = path.find('.')
    if ret > 0:
        return "{}{}{}".format(path[:ret], i, path[ret:])
    return path


def file_send(send_path, recv_path, i, wait_time=0):
    time.sleep(wait_time)
    res = run_command_stdout("{}{}{}{}".format("file send ", send_path, " ", mix_path(recv_path, str(i))))
    print(res)


def file_recv(remote_path, recv_path, i, wait_time=0):
    time.sleep(wait_time)
    res = run_command_stdout("{}{}{}{}".format("file recv ", remote_path, " ", mix_path(recv_path, str(i))))
    print(res)


def get_win_md5(file_path):
    real_md5 = "win_md5"
    send_md5 = run_command_stdout("{}{}{}".format("certutil -hashfile ", os.path.abspath(file_path), " MD5"),
                                  "").split()
    for x in send_md5:
        if len(x) == 32:
            real_md5 = x
    return real_md5


def get_md5(file_path, head=NORMAL_HEAD):
    md5 = run_command_stdout("{}{}{}".format("shell \"md5sum ", file_path, "\""), head)
    return md5.split()[0]


class TestCommands:
    def test_file_cmd(self):
        print("HDC TEST: start test_file_cmd\n")
        for item in TEST_FILES:
            send_file = TEST_FILES[item]['send_file']
            send_file_one = TEST_FILES[item]['send_file_one']
            send_file_two = TEST_FILES[item]['send_file_two']

            recv_file = TEST_FILES[item]['recv_file']
            recv_file_one = TEST_FILES[item]['recv_file_one']
            recv_file_two = TEST_FILES[item]['recv_file_two']

            run_command_stdout("{}{}{}{}".format("file send ", os.path.abspath(PATH['file_empty']['local']), ' ',
                               PATH['file_empty']['remote']), NORMAL_HEAD, False, False)
            run_command_stdout("{}{}{}{}".format("file send ", os.path.abspath(send_file), ' ', recv_file),
                               NORMAL_HEAD, False)
            for i in range(10):
                wait_time = random.uniform(0, 1)
                if i == 0:
                    wait_time = 0
                print("{}{}".format("HDC TEST: start test_file_cmd \n", str(i)))
                send_one = threading.Thread(target=file_send, args=(os.path.abspath(send_file), send_file_one, i))
                send_two = threading.Thread(target=file_send,
                                            args=(os.path.abspath(send_file), send_file_two, i, wait_time))
                recv_one = threading.Thread(target=file_recv, args=(recv_file, os.path.abspath(recv_file_one), i))
                recv_two = threading.Thread(target=file_recv,
                                            args=(recv_file, os.path.abspath(recv_file_two), i, wait_time))

                send_one.start()
                send_two.start()
                recv_one.start()
                recv_two.start()
                send_one.join()
                send_two.join()
                recv_one.join()
                recv_two.join()

    def test_global_server(self):
        print("HDC TEST: start test_global_server_cmd\n")
        run_global_cmd()

    def test_device_cmd(self):
        print("HDC TEST: start test_device_cmd\n")
        devs = get_devs()
        if len(devs) == 1:
            run_device_cmd()
        if len(devs) > 1:
            for dev in devs:
                run_device_cmd("{}{}{}{}".format(NORMAL_HEAD, EXTRA_COMMANDS['choose'], dev, " "))

    def test_tcp_mode(self):
        print("HDC TEST: start test_tcp_mode\n")
        extract_ip()
        global IP
        if len(IP) == 0 or TCP_CFG['ip'] == '':
            print("请连接热点 配置TCP_CFG")
            return
        if tmode_to_tcp():
            commands = get_basic_commands()
            run_commands(commands, NORMAL_HEAD, False)

    def test_remote_server(self):
        time.sleep(10)
        print("HDC TEST: start test_remote_server\n")
        extract_ip()
        ret = run_command_stdout("kill")
        if 'finish' not in ret:
            print('test_remote_server kill server failed')
            return
        global IP
        server_head = "{}{}{}{}{}".format(NORMAL_HEAD, "-s ", IP, ":", "8710 ")
        thread_start = threading.Thread(target=remote_server_start(server_head))
        thread_start.start()
        thread_start.join()
        devs = get_devs(server_head)
        for dev in devs:
            head = "{}{}{}{}".format(server_head, EXTRA_COMMANDS['choose'], dev, " ")
            run_command_stdout("{}{}{}{}".format("file send ", PATH['dir_send']['local'], " ",
                                                 PATH['dir_recv']['remote']), head, False)
            threading.Thread(target=run_split_commands(get_basic_commands(), head)).start()
        run_command("kill", server_head)
