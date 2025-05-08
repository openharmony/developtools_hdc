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

"""
运行环境: python 3.10+
测试方法：
python hdc_stability_test.py

测试范围
hdc shell
hdc file send/recv
hdc fport 文件传输
多session场景测试（hdc tmode + hdc tconn）

注意：
1、当前仅支持一台设备的压力测试
2、如果异常退出，请执行 hdc -t [usb_connect_key] tmode port close，关闭网络接口通道
执行 hdc list targets查看是否还有网络连接，如果有请执行hdc tconn 127.0.0.1:port -remove断开
执行hdc fport ls查看是否还有端口转发的规则，如果有请执行hdc fport rm tcp:xxxxx tcp:9999删除转发
"""

import subprocess
import os
import logging
import logging.config
import time
from enum import Enum
import threading
import uuid
import socket
import hashlib

WORK_DIR = os.getcwd()
LOG_FILE_NAME = f"hdc_stability_test_{time.strftime('%Y%m%d_%H%M%S', time.localtime(time.time()))}.log"

# 资源文件相对路径
RESOURCE_RELATIVE_PATH = "resource"
# 缓存查询到的设备sn
DEVICE_LIST = []
# 记录当前启动的session类信息
SESSION_LIST = []
# 释放资源命令列表
RELEASE_TCONN_CMD_LIST = []
RELEASE_FPORT_CMD_LIST = []

RESOURCE_PATH = os.path.join(WORK_DIR, RESOURCE_RELATIVE_PATH)

# 配置日志记录
logging_config_info = {
    'version': 1,
    'formatters': {
        'format1': {
            'format': '[%(asctime)s %(name)s %(lineno)d %(levelname)s] %(message)s'
        },
    },
    'handlers': {
        'console_handle': {
            'level': 'DEBUG',
            'formatter': 'format1',
            'class': 'logging.StreamHandler',
            'stream': 'ext://sys.stdout',
        },
        'file_handle': {
            'level': 'DEBUG',
            'formatter': 'format1',
            'class': 'logging.FileHandler',
            'filename': LOG_FILE_NAME,
        },
    },
    'loggers': {
        '': {
            'handlers': ['console_handle', 'file_handle'],
            'level': 'DEBUG',
        },
    },
}


class TestType(Enum):
    SHELL = 1
    FILE_SEND = 2
    FILE_RECV = 3
    FPORT_TRANS_FILE = 4


# 循环测试次数
LOOP_COUNT_DEFAULT = 600
# 当前测试任务并行运行的数量
TASK_COUNT_DEFAULT = 6

# 进行多session测试时，启动多个端口转发到设备侧的tmode监听的端口，电脑端开启的端口列表
MUTIL_SESSION_TEST_PC_PORT_LIST = [12345, 12346, 12347, 12348, 12349]
# 多session测试， 通过tmode命令,设备端切换tcp模式，监听的端口号
DEVICE_LISTEN_PORT = 9999


# usb session 测试项配置
USB_SESSION_TEST_CONFIG = [
    {
        "test_type": TestType.SHELL,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "shell ls",
    },
    {
        "test_type": TestType.FILE_SEND,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file send",
        "local": "medium",
        "remote": "/data/medium",
    },
    {
        "test_type": TestType.FILE_RECV,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file recv",
        "original_file": "medium",
        "local": "medium",
        "remote": "/data/medium",
    },
    {
        "test_type": TestType.FPORT_TRANS_FILE,
        # 注意：task_count的值需要和port_info的列表数量对应，即如果有两个任务就需要有两份port_info信息
        "task_count": 2,
        "port_info": [
            {
                "client_connect_port": 22345,
                "daemon_transmit_port": 11081,
                "server_listen_port": 18000,
            },
            {
                "client_connect_port": 22346,
                "daemon_transmit_port": 11082,
                "server_listen_port": 18001,
            },
        ],
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "original_file": "medium",
    },
]


# tcp session 测试项配置
TCP_SESSION_TEST_CONFIG = [
    {
        "test_type": TestType.SHELL,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "shell ls",
    },
    {
        "test_type": TestType.FILE_SEND,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file send",
        "local": "medium",
        "remote": "/data/medium",
    },
    {
        "test_type": TestType.FILE_RECV,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file recv",
        "original_file": "medium",
        "local": "medium",
        "remote": "/data/medium",
    },
]


def init_logger():
    logging.config.dictConfig(logging_config_info)


init_logger()
logger = logging.getLogger(__name__)


def run_cmd_block(command, timeout=600):
    logger.info(f"cmd: {command}")
    # 启动子进程
    process = subprocess.Popen(command.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    # 用于存储子进程的输出
    output = ""
    error = ""

    try:
        # 读取子进程的输出
        output, error = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        logger.info(f"run cmd:{command} timeout")
        process.terminate()
        process.kill()
        output, error = process.communicate(timeout=timeout)
    return output, error


def process_shell_test(connect_key, config, thread_name):
    """
    config格式
    {
        "test_type": TestType.SHELL,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "shell ls",
    }
    """

    logger.info(f"start process_shell_test thread {thread_name}")
    test_count = config["loop_count"]
    delay_s = config["loop_delay_s"]
    cmd = config["cmd"]
    for count in range(test_count):
        logger.info(f"{thread_name} {count}")
        run_cmd_block(f"hdc -t {connect_key} {cmd}")
        time.sleep(delay_s)
    logger.info(f"stop process_shell_test thread {thread_name}")


def process_file_send_test(connect_key, config, thread_name):
    """
    config格式
    {
        "test_type": TestType.FILE_SEND,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file send",
        "local": "medium",
        "remote": "/data/medium",
    }
    """
    logger.info(f"start process_file_send_test thread {thread_name}")
    test_count = config["loop_count"]
    delay_s = config["loop_delay_s"]
    cmd = config["cmd"]
    local_file = os.path.join(RESOURCE_PATH, config['local'])
    remote_file = f"{config['remote']}_{uuid.uuid4()}"
    for count in range(test_count):
        logger.info(f"{thread_name} {count}")
        result, _ = run_cmd_block(f"hdc -t {connect_key} {cmd} {local_file} {remote_file}")
        logger.info(f"{thread_name} {count} result:{result}")
        time.sleep(delay_s)
        run_cmd_block(f"hdc -t {connect_key} shell rm {remote_file}")
    logger.info(f"stop process_file_send_test thread {thread_name}")


def process_file_recv_test(connect_key, config, thread_name):
    """
    config格式
    {
        "test_type": TestType.FILE_RECV,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file recv",
        "original_file": "medium",
        "local": "medium",
        "remote": "/data/medium",
    }
    """
    logger.info(f"start process_file_recv_test thread {thread_name}")
    test_count = config["loop_count"]
    delay_s = config["loop_delay_s"]
    cmd = config["cmd"]
    original_file = os.path.join(RESOURCE_PATH, config['original_file'])
    name_id = uuid.uuid4()
    local_file = os.path.join(RESOURCE_PATH, f"{config['local']}_{name_id}")
    remote_file = f"{config['remote']}_{name_id}"
    run_cmd_block(f"hdc -t {connect_key} file send {original_file} {remote_file}")
    for count in range(test_count):
        logger.info(f"{thread_name} {count}")
        result, _ = run_cmd_block(f"hdc -t {connect_key} {cmd} {remote_file} {local_file}")
        logger.info(f"{thread_name} {count} result:{result}")
        time.sleep(delay_s)
        os.remove(local_file)
        logger.debug(f"{connect_key} remove {local_file} finished")
    run_cmd_block(f"hdc -t {connect_key} shell rm {remote_file}")
    logger.info(f"stop process_file_recv_test thread {thread_name}")


def send_file(conn, file_name):
    logger.info(f"send_file enter:{file_name}")
    file_path = os.path.join(RESOURCE_PATH, file_name)
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
        logger.info(f"send_file: recv check size len is 0, exit")
        return
    file_size_recv = int(size_raw.decode('utf-8'))
    if file_size_recv != file_size:
        logger.info(f"send_file: check size failed, file_size_recv:{file_size_recv} file size:{file_size}")
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

        file_path = os.path.join(RESOURCE_PATH, file_save_name)
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


def get_local_md5(local):
    md5_hash = hashlib.md5()
    with open(local, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            md5_hash.update(byte_block)
    return md5_hash.hexdigest()


def process_fport_trans_file_test(connect_key, config, thread_name, task_index):
    """
    config格式
    {
        "test_type": TestType.FPORT_TRANS_FILE,
        "task_count": TASK_COUNT_DEFAULT,
        "port_info": [
            {
                "client_connect_port": 22345,
                "daemon_transmit_port": 11081,
                "server_listen_port": 18000,
            },
            {
                "client_connect_port": 22346,
                "daemon_transmit_port": 11082,
                "server_listen_port": 18001,
            },
        ],

        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "original_file": "medium",
    }
    """
    logger.info(f"start process_fport_trans_file_test thread {thread_name}")
    test_count = config["loop_count"]
    delay_s = config["loop_delay_s"]
    client_connect_port = config["port_info"][task_index]["client_connect_port"]
    daemon_transmit_port = config["port_info"][task_index]["daemon_transmit_port"]
    server_listen_port = config["port_info"][task_index]["server_listen_port"]

    # 构建传输通道
    fport_arg = f"tcp:{client_connect_port} tcp:{daemon_transmit_port}"
    result, _ = run_cmd_block(f"hdc -t {connect_key} fport {fport_arg}")
    logger.info(f"{thread_name} fport result:{result}")
    rport_arg = f"tcp:{daemon_transmit_port} tcp:{server_listen_port}"
    result, _ = run_cmd_block(f"hdc -t {connect_key} rport {rport_arg}")
    logger.info(f"{thread_name} rport result:{result}")

    # transmit file start
    file_name = config["original_file"]
    name_id = uuid.uuid4()
    file_save_name = f"{file_name}_recv_fport_{name_id}"
    file_path = os.path.join(RESOURCE_PATH, file_save_name)
    if os.path.exists(file_path):
        logger.info(f"client: file {file_path} exist, delete")
        try:
            os.remove(file_path)
        except OSError as error:
            logger.error(f"delete {file_path} failed: {error.strerror}")
            run_cmd_block(f"hdc -t {connect_key} fport rm {fport_arg}")
            run_cmd_block(f"hdc -t {connect_key} fport rm {rport_arg}")
            return

    for count in range(test_count):
        logger.info(f"{thread_name} {count}")
        server_thread = threading.Thread(target=server_loop, args=(server_listen_port,))
        server_thread.start()

        client_get_file(client_connect_port, file_name, file_save_name)
        server_thread.join()

        ori_file_md5 = get_local_md5(os.path.join(RESOURCE_PATH, file_name))
        new_file = os.path.join(RESOURCE_PATH, file_save_name)
        if os.path.exists(new_file):
            new_file_md5 = get_local_md5(new_file)
            os.remove(new_file)
        logger.info(f"ori_file_md5:{ori_file_md5}, new_file_md5:{new_file_md5}")

        if not ori_file_md5 == new_file_md5:
            logger.error(f"check file md5 failed, ori_file_md5:{ori_file_md5}, new_file_md5:{new_file_md5}")
            # 关闭fport通道
            run_cmd_block(f"hdc -t {connect_key} fport rm {fport_arg}")
            run_cmd_block(f"hdc -t {connect_key} fport rm {rport_arg}")
            return

    # 关闭fport通道
    run_cmd_block(f"hdc -t {connect_key} fport rm {fport_arg}")
    run_cmd_block(f"hdc -t {connect_key} fport rm {rport_arg}")
    logger.info(f"stop process_fport_trans_file_test thread {thread_name}")


class Session(object):
    """
    session class
    一个session连接对应一个session class
    包含了所有在当前连接下面的测试任务
    """
    # session类型，usb or tcp
    session_type = ""
    # 设备连接标识
    connect_key = ""
    test_config = []
    thread_list = []

    def __init__(self, connect_key):
        self.connect_key = connect_key

    def set_test_config(self, config):
        self.test_config = config

    def start_test(self):
        if len(self.test_config) == 0:
            logger.error(f"start test test_config is empty, type:{self.session_type} {self.connect_key}")
            return False
        for one_config in self.test_config:
            if self.start_one_test(one_config) is False:
                logger.error(f"start one test failed, type:{self.session_type} {self.connect_key} config:{one_config}")
                return False
        return True

    def start_one_test(self, config):
        if config["test_type"] == TestType.SHELL:
            if self.start_shell_test(config) is False:
                logger.error(f"start_shell_test failed, type:{self.session_type} {self.connect_key} config:{config}")
                return False
        if config["test_type"] == TestType.FILE_SEND:
            if self.start_file_send_test(config) is False:
                logger.error(f"start_file_send_test failed, type:{self.session_type} \
                    {self.connect_key} config:{config}")
                return False
        if config["test_type"] == TestType.FILE_RECV:
            if self.start_file_recv_test(config) is False:
                logger.error(f"start_file_recv_test failed, type:{self.session_type} \
                    {self.connect_key} config:{config}")
                return False
        if config["test_type"] == TestType.FPORT_TRANS_FILE:
            if self.start_fport_trans_file_test(config) is False:
                logger.error(f"start_fport_trans_file_test failed, type:{self.session_type} \
                    {self.connect_key} config:{config}")
                return False
        return True

    def start_shell_test(self, config):
        task_count = config["task_count"]
        test_name = f"{self.connect_key}_shell"
        for task_index in range(task_count):
            thread = threading.Thread(target=process_shell_test, name=f"{test_name}_{task_index}",
                                      args=(self.connect_key, config, f"{test_name}_{task_index}"))
            thread.start()
            self.thread_list.append(thread)

    def start_file_send_test(self, config):
        task_count = config["task_count"]
        test_name = f"{self.connect_key}_filesend"
        for task_index in range(task_count):
            thread = threading.Thread(target=process_file_send_test, name=f"{test_name}_{task_index}",
                                      args=(self.connect_key, config, f"{test_name}_{task_index}"))
            thread.start()
            self.thread_list.append(thread)

    def start_file_recv_test(self, config):
        task_count = config["task_count"]
        test_name = f"{self.connect_key}_filerecv"
        for task_index in range(task_count):
            thread = threading.Thread(target=process_file_recv_test, name=f"{test_name}_{task_index}",
                                      args=(self.connect_key, config, f"{test_name}_{task_index}"))
            thread.start()
            self.thread_list.append(thread)

    def start_fport_trans_file_test(self, config):
        task_count = config["task_count"]
        test_name = f"{self.connect_key}_fporttrans"
        for task_index in range(task_count):
            thread = threading.Thread(target=process_fport_trans_file_test, name=f"{test_name}_{task_index}",
                                      args=(self.connect_key, config, f"{test_name}_{task_index}", task_index))
            thread.start()
            self.thread_list.append(thread)


class UsbSession(Session):
    """
    usb连接对应的session类
    """
    session_type = "usb"


class TcpSession(Session):
    """
    tcp连接对应的session类
    """
    session_type = "tcp"

    def start_tcp_connect(self):
        result, _ = run_cmd_block(f"hdc tconn {self.connect_key}")
        logger.info(f"start_tcp_connect {self.connect_key} result:{result}")
        RELEASE_TCONN_CMD_LIST.append(f"hdc tconn {self.connect_key} -remove")
        return True


def start_server():
    cmd = "hdc start"
    result, _ = run_cmd_block(cmd)
    return result


def get_dev_list():
    try:
        result, _ = run_cmd_block("hdc list targets")
        result = result.split()
    except (OSError, IndexError):
        result = ["failed to detect device"]
        return False, result
    targets = result
    if len(targets) == 0:
        logger.error(f"get device, devices list is empty")
        return False, []
    if "[Empty]" in targets[0]:
        logger.error(f"get device, no devices found, devices:{targets}")
        return False, targets
    return True, targets


def check_device_online(connect_key):
    """
    确认设备是否在线
    """
    result, devices = get_dev_list()
    if result is False:
        logger.error(f"get device failed, devices:{devices}")
        return False
    if connect_key in devices:
        return True
    return False


def init_test_env():
    """
    初始化测试环境
    1、使用tmode port和fport转发，构建多session tcp测试场景
    """
    logger.info(f"init env")
    start_server()
    result, devices = get_dev_list()
    if result is False:
        logger.error(f"get device failed, devices:{devices}")
        return False
    device_sn = devices[0]
    logger.info(f"get device:{device_sn}")

    # 开启设备侧监听端口
    result, _ = run_cmd_block(f"hdc -t {device_sn} tmode port {DEVICE_LISTEN_PORT}")
    logger.info(f"run tmode result:{result}")
    time.sleep(10)
    logger.info(f"start check device:{device_sn}")
    if check_device_online(device_sn) is False:
        logger.error(f"device {device_sn} in not online")
        return False
    logger.info(f"init_test_env finished")
    return True


def start_usb_session_test(connect_key):
    """
    开始一个usb session的测试
    """
    logger.info(f"start_usb_session_test connect_key:{connect_key}")
    session = UsbSession(connect_key)
    session.set_test_config(USB_SESSION_TEST_CONFIG)
    if session.start_test() is False:
        logger.error(f"session.start_test failed, connect_key:{connect_key}")
        return False
    SESSION_LIST.append(session)
    logger.info(f"start_usb_session_test connect_key:{connect_key} finished")
    return True


def start_local_tcp_session_test(connect_key, port_list):
    """
    1、遍历传入的端口号，创建fport端口转发到设备端的DEVICE_LISTEN_PORT，
    2、使用tconn连接后进行测试
    """
    logger.info(f"start_local_tcp_session_test port_list:{port_list}")
    for port in port_list:
        # 构建fport通道
        logger.info(f"create fport local:{port} device:{DEVICE_LISTEN_PORT}")
        result, _ = run_cmd_block(f"hdc -t {connect_key} fport tcp:{port} tcp:{DEVICE_LISTEN_PORT}")
        logger.info(f"create fport local:{port} device:{DEVICE_LISTEN_PORT} result:{result}")
        RELEASE_FPORT_CMD_LIST.append(f"hdc -t {connect_key} fport rm tcp:{port} tcp:{DEVICE_LISTEN_PORT}")
        tcp_key = f"127.0.0.1:{port}"
        logger.info(f"start TcpSession connect_key:{tcp_key}")
        session = TcpSession(tcp_key)
        session.set_test_config(TCP_SESSION_TEST_CONFIG)
        session.start_tcp_connect()
        if session.start_test() is False:
            logger.error(f"session.start_test failed, connect_key:{tcp_key}")
            return False
        SESSION_LIST.append(session)
        logger.info(f"start tcp test connect_key:{tcp_key} finished")

    logger.info(f"start_local_tcp_session_test finished")
    return True


def start_test():
    """
    启动测试
    1、启动usb session场景的测试
    2、启动通过 tmode+fport模拟的tcp session场景的测试
    """
    logger.info(f"start test")
    result, devices = get_dev_list()
    if result is False:
        logger.error(f"get device failed, devices:{devices}")
        return False
    device_sn = devices[0]
    DEVICE_LIST.append(device_sn)
    if start_usb_session_test(device_sn) is False:
        logger.error(f"start_usb_session_test failed, devices:{devices}")
        return False
    if start_local_tcp_session_test(device_sn, MUTIL_SESSION_TEST_PC_PORT_LIST) is False:
        logger.error(f"start_tcp_session_test failed, devices:{devices}")
        return False
    return True


def release_resource():
    """
    释放相关资源
    1、断开tconn连接
    2、fport转发
    3、tmode port
    """
    logger.info(f"enter release_resource")
    for cmd in RELEASE_TCONN_CMD_LIST:
        result, _ = run_cmd_block(cmd)
        logger.info(f"release tconn cmd:{cmd} result:{result}")
    for cmd in RELEASE_FPORT_CMD_LIST:
        result, _ = run_cmd_block(cmd)
        logger.info(f"release fport cmd:{cmd} result:{result}")
    result, _ = run_cmd_block(f"hdc -t {DEVICE_LIST[0]} tmode port close")
    logger.info(f"tmode port close, device:{DEVICE_LIST[0]} result:{result}")


def main():
    logger.info(f"main resource_path:{RESOURCE_PATH}")
    if init_test_env() is False:
        logger.error(f"init_test_env failed")
        return
    if start_test() is False:
        logger.error(f"start_test failed")
        return

    # wait all thread exit
    for session in SESSION_LIST:
        for thread in session.thread_list:
            thread.join()
    release_resource()
    logger.info(f"exit main")


if __name__ == "__main__":
    main()
