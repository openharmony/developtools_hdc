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

"""
运行环境: python 3.10+
测试方法：
方法1、使用pytest框架执行
请见测试用例根目录test\scripts下readme.md中的执行稳定性用例小节
方法2、直接执行
进入test\scripts目录，执行python .\testModule\stability_utils.py

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
from datetime import datetime
from enum import Enum
import threading
import uuid
import hashlib
import queue
from utils import server_loop
from utils import client_get_file
from utils import get_local_md5

STABILITY_TEST_VERSION = "v1.0.0"

TIME_REPORT_FORMAT = '%Y-%m-%d %H:%M:%S'
TIME_FILE_FORMAT = '%Y%m%d_%H%M%S'
TEST_RESULT = False

WORK_DIR = os.getcwd()

# 消息队列中使用到的key名称
# 报告公共信息key名称
REPORT_PUBLIC_INFO_NAME = "public_info"
# 报告公共信息中的错误信息key名称
TASK_ERROR_KEY_NAME = "task_error"

# 资源文件相对路径
RESOURCE_RELATIVE_PATH = "resource"
# 报告文件相对路径
REPORTS_RELATIVE_PATH = "reports"
# 缓存查询到的设备sn
DEVICE_LIST = []
# 记录当前启动的session类信息
SESSION_LIST = []
# 释放资源命令列表
RELEASE_TCONN_CMD_LIST = []
RELEASE_FPORT_CMD_LIST = []

RESOURCE_PATH = os.path.join(WORK_DIR, RESOURCE_RELATIVE_PATH)

EXIT_EVENT = threading.Event()

TEST_THREAD_MSG_QUEUE = queue.Queue()


def get_time_str(fmt=TIME_REPORT_FORMAT, need_ms=False):
    now_time = datetime.now()
    if need_ms is False:
        return now_time.strftime(fmt)
    else:
        return f"{now_time.strftime(fmt)}.{now_time.microsecond // 1000:03d}"


TEST_FILE_TIME_STR = get_time_str(TIME_FILE_FORMAT)
LOG_FILE_NAME = os.path.join(REPORTS_RELATIVE_PATH, f"hdc_stability_test_{TEST_FILE_TIME_STR}.log")
REPORT_FILE_NAME = os.path.join(REPORTS_RELATIVE_PATH, f"hdc_stability_test_report_{TEST_FILE_TIME_STR}.html")
logger = None

# 配置日志记录
logging_config_info = {
    'version': 1,
    'formatters': {
        'format1': {
            'format': '[%(asctime)s %(name)s %(funcName)s %(lineno)d %(levelname)s][%(process)d][%(thread)d]'
                      '[%(threadName)s]%(message)s'
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


# 当前测试任务并行运行的数量
TASK_COUNT_DEFAULT = 5
# 循环测试次数
LOOP_COUNT_DEFAULT = 20
# 每个循环结束的等待时间，单位秒
LOOP_DELAY_S_DEFAULT = 0.01

# 进行多session测试时，启动多个端口转发到设备侧的tmode监听的端口，电脑端开启的端口列表
# 参考端口配置：12345, 12346, 12347, 12348, 12349，需要几个session，可以复制几个放入下面的MUTIL_SESSION_TEST_PC_PORT_LIST中
MUTIL_SESSION_TEST_PC_PORT_LIST = []
# 多session测试， 通过tmode命令,设备端切换tcp模式，监听的端口号
DEVICE_LISTEN_PORT = 9999

# usb session 测试项配置
"""
配置项包括如下：
    {
        "test_type": TestType.SHELL,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "shell ls",
        "expected_results": "data",
    },
    {
        "test_type": TestType.FILE_SEND,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file send",
        "local": "medium",
        "remote": "/data/medium",
        "expected_results": "FileTransfer finish",
    },
    {
        "test_type": TestType.FILE_RECV,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file recv",
        "original_file": "medium",
        "local": "medium",
        "remote": "/data/medium",
        "expected_results": "FileTransfer finish",
    },
    {
        "test_type": TestType.FPORT_TRANS_FILE,
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
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "original_file": "medium",
    },
"""
USB_SESSION_TEST_CONFIG = [
    {
        "test_type": TestType.SHELL,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "shell ls",
        "expected_results": "data",
    },
    {
        "test_type": TestType.FILE_SEND,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file send",
        "local": "medium",
        "remote": "/data/medium",
        "expected_results": "FileTransfer finish",
    },
    {
        "test_type": TestType.FILE_RECV,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file recv",
        "original_file": "medium",
        "local": "medium",
        "remote": "/data/medium",
        "expected_results": "FileTransfer finish",
    },
]

# tcp session 测试项配置
"""
配置项包括如下：
    {
        "test_type": TestType.SHELL,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "shell ls",
        "expected_results": "data",
    },
    {
        "test_type": TestType.FILE_SEND,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file send",
        "local": "medium",
        "remote": "/data/medium",
        "expected_results": "FileTransfer finish",
    },
    {
        "test_type": TestType.FILE_RECV,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file recv",
        "original_file": "medium",
        "local": "medium",
        "remote": "/data/medium",
        "expected_results": "FileTransfer finish",
    },
"""
TCP_SESSION_TEST_CONFIG = [
    {
        "test_type": TestType.SHELL,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "shell ls",
        "expected_results": "data",
    },
    {
        "test_type": TestType.FILE_SEND,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file send",
        "local": "medium",
        "remote": "/data/medium",
        "expected_results": "FileTransfer finish",
    },
    {
        "test_type": TestType.FILE_RECV,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": LOOP_DELAY_S_DEFAULT,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "file recv",
        "original_file": "medium",
        "local": "medium",
        "remote": "/data/medium",
        "expected_results": "FileTransfer finish",
    },
]

HTML_HEAD = """
<head>
<meta charset="UTF-8">
<title>HDC稳定性测试报告</title>
<style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; }
    table { border-collapse: collapse; width: 100%; }
    th { background-color: #f0f0f0; }
    th, td { border: 1px solid #ddd; padding: 6px; text-align: left; }
    tr:nth-child(even) { background-color: #f9f9f9; }
    .report {
        max-width: 1200px;
        margin: 0 auto;
        background-color: #fafafa;
        padding: 20px;
        border-radius: 5px;
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.09);
        color: #333;
    }
    .summary {
        margin-bottom: 10px;
        padding: 20px;
        background-color: #fff;
        border-radius: 5px;
        box-shadow: 0 2px 2px rgba(0, 0, 0, 0.05);
    }
    .summary-header { margin-bottom: 10px; padding-bottom: 5px; }
    .summary-row {
        display: flex;
        justify-content: space-between;
        margin-bottom: 2px;
        flex-wrap: wrap;
    }
    .summary-item {
        flex: 1;
        min-width: 300px;
        margin: 4px;
        padding: 7px;
        background-color: #f2f2f2;
        border-radius: 4px;
    }
    .summary-item-key {
        font-weight: bold;
        margin-bottom: 3px;
        display: block;
    }
    .summary-item-value { color: #444; }
    .detail {
        padding: 20px;
        background-color: #fff;
        border-radius: 5px;
        box-shadow: 0 2px 2px rgba(0, 0, 0, 0.05);
    }
    .detail-header {
        margin-bottom: 10px;
        border-bottom: 1px solid #eee;
        padding-bottom: 10px;
    }
    .pass { color: green; }
    .fail { color: red; }
</style>
</head>
"""


def init_logger():
    logging.config.dictConfig(logging_config_info)


def put_msg(queue_obj, info):
    """
    给队列中填入测试报告信息，用于汇总报告和生成结果
    """
    queue_obj.put(info)


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


def run_cmd_and_check_output(command, want_str, timeout=600):
    """
    阻塞的方式运行命令，然后判断标准输出的返回值中是否有预期的字符串
    存在预期的字符串返回 True
    不存在预期的字符串返回 False
    """
    output, error = run_cmd_block(command, timeout)
    if want_str in output:
        return True, (output, error)
    else:
        logger.error(f"can not get expect str:{want_str} cmd:{command} output:{output} error:{error}")
        return False, (output, error)


def process_shell_test(connect_key, config, thread_name):
    """
    config格式
    {
        "test_type": TestType.SHELL,
        "loop_count": LOOP_COUNT_DEFAULT,
        "loop_delay_s": 0.01,
        "task_count": TASK_COUNT_DEFAULT,
        "cmd": "shell ls",
        "expected_results": "data",
    }
    """
    logger.info(f"start process_shell_test thread {thread_name}")
    test_count = config["loop_count"]
    delay_s = config["loop_delay_s"]
    cmd = f"hdc -t {connect_key} {config['cmd']}"
    expected_results = config["expected_results"]
    report = {"test_type": config["test_type"], "test_name": thread_name, "loop_count": test_count}
    start_time = time.perf_counter()
    for count in range(test_count):
        logger.info(f"{thread_name} {count}")
        is_ok, info = run_cmd_and_check_output(cmd, expected_results)
        if is_ok:
            logger.info(f"{thread_name} {count} success result:{info}")
        else:
            logger.error(f"{thread_name} loop_count:{count+1} run:{cmd} can not get expect result:{expected_results},"
                         f"result:{info}")
            error_time = get_time_str(need_ms=True)
            msg = f"<span style='color: #0000ff;'>[{error_time}]</span> {thread_name} loop_count:{count+1} run:{cmd}" \
                  f" can not get expect result:{expected_results}, result:{info}"
            report["error_msg"] = f"[{error_time}] result:{info}"
            TEST_THREAD_MSG_QUEUE.put({TASK_ERROR_KEY_NAME: {"name": thread_name, "error_msg": msg}})
            EXIT_EVENT.set()
            logger.info(f"{thread_name} {count} set exit event")
            report["finished_test_count"] = count
            break
        if EXIT_EVENT.is_set():
            logger.info(f"{thread_name} {count} exit event is set")
            report["error_msg"] = "event is set, normal exit"
            report["finished_test_count"] = count + 1
            break
        time.sleep(delay_s)
    if "finished_test_count" not in report:
        report["finished_test_count"] = test_count
    if report["finished_test_count"] < test_count:
        report["passed"] = False
    else:
        report["passed"] = True
    elapsed_time = time.perf_counter() - start_time
    report["cost_time"] = elapsed_time
    TEST_THREAD_MSG_QUEUE.put({thread_name: report})
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
        "expected_results": "FileTransfer finish",
    }
    """
    logger.info(f"start process_file_send_test thread {thread_name}")
    test_count = config["loop_count"]
    delay_s = config["loop_delay_s"]
    expected_results = config["expected_results"]
    local_file = os.path.join(RESOURCE_PATH, config['local'])
    remote_file = f"{config['remote']}_{uuid.uuid4()}"
    cmd = f"hdc -t {connect_key} {config['cmd']} {local_file} {remote_file}"
    report = {"test_type": config["test_type"], "test_name": thread_name, "loop_count": test_count}
    start_time = time.perf_counter()
    for count in range(test_count):
        logger.info(f"{thread_name} {count}")
        is_ok, info = run_cmd_and_check_output(cmd, expected_results)
        if is_ok:
            logger.info(f"{thread_name} {count} success result:{info}")
        else:
            logger.error(f"{thread_name} loop_count:{count+1} run:{cmd} can not get expect result:{expected_results},"
                         f"result:{info}")
            error_time = get_time_str(need_ms=True)
            msg = f"<span style='color: #0000ff;'>[{error_time}]</span> {thread_name} loop_count:{count+1} run:{cmd}" \
                  f" can not get expect result:{expected_results}, result:{info}"
            report["error_msg"] = f"[{error_time}] result:{info}"
            TEST_THREAD_MSG_QUEUE.put({TASK_ERROR_KEY_NAME: {"name": thread_name, "error_msg": msg}})
            EXIT_EVENT.set()
            logger.info(f"{thread_name} {count} set exit event")
            report["finished_test_count"] = count
            break

        run_cmd_block(f"hdc -t {connect_key} shell rm {remote_file}")
        if EXIT_EVENT.is_set():
            logger.info(f"{thread_name} {count} exit event is set")
            report["error_msg"] = "event is set, normal exit"
            report["finished_test_count"] = count + 1
            break
        time.sleep(delay_s)
    if "finished_test_count" not in report:
        report["finished_test_count"] = test_count
    if report["finished_test_count"] < test_count:
        report["passed"] = False
    else:
        report["passed"] = True
    elapsed_time = time.perf_counter() - start_time
    report["cost_time"] = elapsed_time
    TEST_THREAD_MSG_QUEUE.put({thread_name: report})
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
        "expected_results": "FileTransfer finish",
    }
    """
    logger.info(f"start process_file_recv_test thread {thread_name}")
    test_count = config["loop_count"]
    delay_s = config["loop_delay_s"]
    expected_results = config["expected_results"]
    original_file = os.path.join(RESOURCE_PATH, config['original_file'])
    name_id = uuid.uuid4()
    local_file = os.path.join(RESOURCE_PATH, f"{config['local']}_{name_id}")
    remote_file = f"{config['remote']}_{name_id}"
    cmd = f"hdc -t {connect_key} {config['cmd']} {remote_file} {local_file}"
    report = {"test_type": config["test_type"], "test_name": thread_name, "loop_count": test_count}
    start_time = time.perf_counter()
    run_cmd_block(f"hdc -t {connect_key} file send {original_file} {remote_file}")
    for count in range(test_count):
        logger.info(f"{thread_name} {count}")
        is_ok, info = run_cmd_and_check_output(cmd, expected_results)
        if is_ok:
            logger.info(f"{thread_name} {count} success result:{info}")
        else:
            logger.error(f"{thread_name} loop_count:{count+1} run:{cmd} can not get expect result:{expected_results},"
                         f"result:{info}")
            error_time = get_time_str(need_ms=True)
            msg = f"<span style='color: #0000ff;'>[{error_time}]</span> {thread_name} loop_count:{count+1} run:{cmd}" \
                  f" can not get expect result:{expected_results}, result:{info}"
            report["error_msg"] = f"[{error_time}] result:{info}"
            TEST_THREAD_MSG_QUEUE.put({TASK_ERROR_KEY_NAME: {"name": thread_name, "error_msg": msg}})
            EXIT_EVENT.set()
            logger.info(f"{thread_name} {count} set exit event")
            report["finished_test_count"] = count
            break
        os.remove(local_file)
        logger.debug(f"{connect_key} remove {local_file} finished")
        if EXIT_EVENT.is_set():
            logger.info(f"{thread_name} {count} exit event is set")
            report["error_msg"] = "event is set, normal exit"
            report["finished_test_count"] = count + 1
            break
        time.sleep(delay_s)
    if "finished_test_count" not in report:
        report["finished_test_count"] = test_count
    if report["finished_test_count"] < test_count:
        report["passed"] = False
    else:
        report["passed"] = True
    run_cmd_block(f"hdc -t {connect_key} shell rm {remote_file}")
    elapsed_time = time.perf_counter() - start_time
    report["cost_time"] = elapsed_time
    TEST_THREAD_MSG_QUEUE.put({thread_name: report})
    logger.info(f"stop process_file_recv_test thread {thread_name}")


def create_fport_trans_test_env(info):
    # 构建传输通道
    thread_name = info.get("thread_name")
    connect_key = info.get("connect_key")
    fport_arg = info.get("fport_arg")
    result, _ = run_cmd_block(f"hdc -t {connect_key} fport {fport_arg}")
    logger.info(f"{thread_name} fport result:{result}")
    rport_arg = info.get("rport_arg")
    result, _ = run_cmd_block(f"hdc -t {connect_key} rport {rport_arg}")
    logger.info(f"{thread_name} rport result:{result}")


def do_fport_trans_file_test_once(client_connect_port, server_listen_port, file_name, file_save_name):
    """
    进行一次数据传输测试
    """
    logger.info(f"do_fport_trans_file_test_once start, file_save_name:{file_save_name}")
    server_thread = threading.Thread(target=server_loop, args=(server_listen_port,))
    server_thread.start()

    client_get_file(client_connect_port, file_name, file_save_name)
    server_thread.join()

    ori_file_md5 = get_local_md5(os.path.join(RESOURCE_PATH, file_name))
    new_file = os.path.join(RESOURCE_PATH, file_save_name)
    new_file_md5 = 0
    if os.path.exists(new_file):
        new_file_md5 = get_local_md5(new_file)
        os.remove(new_file)
    logger.info(f"ori_file_md5:{ori_file_md5}, new_file_md5:{new_file_md5}")

    if not ori_file_md5 == new_file_md5:
        logger.error(f"check file md5 failed, file_save_name:{file_save_name}, ori_file_md5:{ori_file_md5},"
                     f"new_file_md5:{new_file_md5}")
        return False
    logger.info(f"do_fport_trans_file_test_once exit, file_save_name:{file_save_name}")
    return True


def process_fport_trans_file_test(connect_key, config, thread_name, task_index):
    """
    task_index对应当前测试的port_info，从0开始计数。
    config格式
    {
        "test_type": TestType.FPORT_TRANS_FILE,
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
    port_info = config["port_info"]
    client_connect_port = port_info[task_index]["client_connect_port"]
    daemon_transmit_port = port_info[task_index]["daemon_transmit_port"]
    server_listen_port = port_info[task_index]["server_listen_port"]
    fport_arg = f"tcp:{client_connect_port} tcp:{daemon_transmit_port}"
    rport_arg = f"tcp:{daemon_transmit_port} tcp:{server_listen_port}"

    report = {"test_type": config["test_type"], "test_name": thread_name, "loop_count": test_count}
    start_time = time.perf_counter()

    # 构建传输通道
    create_fport_trans_test_env({"thread_name": thread_name, "connect_key": connect_key,
        "fport_arg": fport_arg, "rport_arg": rport_arg})

    # transmit file start
    file_name = config["original_file"]
    file_save_name = f"{file_name}_recv_fport_{uuid.uuid4()}"
    for count in range(test_count):
        logger.info(f"{thread_name} {count}")
        if do_fport_trans_file_test_once(client_connect_port, server_listen_port, file_name, file_save_name) is False:
            error_time = get_time_str(need_ms=True)
            msg = f"<span style='color: #0000ff;'>[{error_time}]</span> {thread_name} loop_count:{count+1}" \
                  f" check file md5 failed"
            report["error_msg"] = f"[{error_time}] check file md5 failed"
            TEST_THREAD_MSG_QUEUE.put({TASK_ERROR_KEY_NAME: {"name": thread_name, "error_msg": msg}})
            EXIT_EVENT.set()
            logger.info(f"{thread_name} {count} set exit event")
            report["finished_test_count"] = count
            break
        if EXIT_EVENT.is_set():
            logger.info(f"{thread_name} {count} exit event is set")
            report["error_msg"] = "event is set, normal exit"
            report["finished_test_count"] = count + 1
            break
        time.sleep(delay_s)

    # 关闭fport通道
    run_cmd_block(f"hdc -t {connect_key} fport rm {fport_arg}")
    run_cmd_block(f"hdc -t {connect_key} fport rm {rport_arg}")
    if "finished_test_count" not in report:
        report["finished_test_count"] = test_count
    if report["finished_test_count"] < test_count:
        report["passed"] = False
    else:
        report["passed"] = True
    elapsed_time = time.perf_counter() - start_time
    report["cost_time"] = elapsed_time
    TEST_THREAD_MSG_QUEUE.put({thread_name: report})
    logger.info(f"stop process_fport_trans_file_test thread {thread_name}")


def get_test_result(fail_num):
    """
    返回错误结果及错误结果显示类型信息
    """
    global TEST_RESULT
    if fail_num > 0:
        test_result = "失败"
        TEST_RESULT = False
        test_result_class = "fail"
    else:
        test_result = "通过"
        TEST_RESULT = True
        test_result_class = "pass"
    return test_result, test_result_class


def get_error_msg_html(public_info):
    """
    获取html格式的错误信息
    """
    error_msg_list = public_info.get(TASK_ERROR_KEY_NAME)
    error_msg_html = ''
    if error_msg_list is not None:
        error_msg = '\n'.join(error_msg_list)
        error_msg_html = f"""
            <div class="summary-row">
                <div class="summary-item">
                    <span class="summary-item-key">错误信息</span>
                    <span class="summary-item-value fail">{error_msg}</span>
                </div>
            </div>
        """
    return error_msg_html


def gen_report_public_info(public_info):
    """
    生成html格式的报告公共信息部分
    """
    start_time = public_info.get('start_time')
    stop_time = public_info.get('stop_time')
    pass_num = public_info.get('pass_num')
    fail_num = public_info.get('fail_num')
    test_task_num = pass_num + fail_num
    test_result, test_result_class = get_test_result(fail_num)

    # 错误信息
    error_msg_html = get_error_msg_html(public_info)

    public_html_temp = f"""
        <div class="summary-row">
            <div class="summary-item">
                <span class="summary-item-key">用例版本号</span>
                <span class="summary-item-value">{STABILITY_TEST_VERSION}</span>
            </div>
            <div class="summary-item">
                <span class="summary-item-key">开始时间</span>
                <span class="summary-item-value">{start_time}</span>
            </div>
            <div class="summary-item">
                <span class="summary-item-key">结束时间</span>
                <span class="summary-item-value">{stop_time}</span>
            </div>
        </div>
        <div class="summary-row">
            <div class="summary-item">
                <span class="summary-item-key">测试任务数</span>
                <span class="summary-item-value">{test_task_num}</span>
            </div>
            <div class="summary-item">
                <span class="summary-item-key">失败任务数</span>
                <span class="summary-item-value fail">{fail_num}</span>
            </div>
            <div class="summary-item">
                <span class="summary-item-key">成功任务数</span>
                <span class="summary-item-value pass">{pass_num}</span>
            </div>
        </div>
        <div class="summary-row">
            <div class="summary-item">
                <span class="summary-item-key">测试结果</span>
                <span class="summary-item-value {test_result_class}">{test_result}</span>
            </div>
        </div>
        {error_msg_html}
    """
    return public_html_temp


def gen_report_detail_info(pass_list, fail_list):
    """
    生成html格式的报告详细信息部分
    pass_list fail_list 格式：[("测试名称", {"key": value, "key": value, "key": value, "key": value ...}), ... ]
    """
    # 生成表格每一行记录
    detail_rows_list = []
    for one_test_key, one_test_value in fail_list + pass_list:
        if one_test_value.get("passed") is True:
            result_class = "pass"
            result_text = "通过"
        else:
            result_class = "fail"
            result_text = "失败"
        finished_test_count = one_test_value.get("finished_test_count")
        loop_count = one_test_value.get("loop_count")
        completion_rate = 100.0 * finished_test_count / loop_count
        cost_time = one_test_value.get("cost_time")
        if finished_test_count > 0:
            cost_time_per_test = f"{cost_time / finished_test_count:.3f}"
        else:
            cost_time_per_test = "NA"
        one_row = f"""
            <tr>
                <td>{one_test_key}</td>
                <td class="{result_class}">{result_text}</td>
                <td>{completion_rate:.1f}</td>
                <td>{finished_test_count}</td>
                <td>{loop_count}</td>
                <td>{cost_time_per_test}</td>
                <td>{cost_time:.3f}</td>
                <td>{one_test_value.get("error_msg", "NA")}</td>
            </tr>
        """
        detail_rows_list.append(one_row)
    detail_rows = ''.join(detail_rows_list)

    # 合成表格
    detail_table_temp = f"""
        <table>
            <tr>
                <th>任务名称</th>
                <th>测试结果</th>
                <th>完成率</th>
                <th>已完成次数</th>
                <th>总次数</th>
                <th>平均每轮耗时（秒）</th>
                <th>总耗时（秒）</th>
                <th>错误信息</th>
            </tr>
            {detail_rows}
        </table>
    """
    return detail_table_temp


def gen_report_info(public_info, detail_info):
    """
    生成html格式的报告
    """
    html_temp = f"""
    <!DOCTYPE html>
    <html lang="zh-CN">
    {HTML_HEAD}
    <body>
        <div class="report">
            <section class="summary">
                <h1 style="text-align: center;">HDC稳定性测试报告</h1>
                <h2 class="summary-header">概要</h2>
                {public_info}
            </section>
            <section class="detail">
                <h2 class="detail-header">详情</h2>
                {detail_info}
            </section>
        </div>
    </body>
    </html>
    """
    return html_temp


def save_report(report_info, save_file_name):
    """
    生成测试报告
    """
    # 字典中获取报告公共信息
    public_info = report_info.get(REPORT_PUBLIC_INFO_NAME)
    # 字典中获取详细测试项目的信息
    detail_test_info = {key: value for key, value in report_info.items() if key != REPORT_PUBLIC_INFO_NAME}

    # 分类测试结果
    pass_list = []
    fail_list = []
    for one_test_key, one_test_value in detail_test_info.items():
        if one_test_value.get("passed") is True:
            pass_list.append((one_test_key, one_test_value))
        else:
            fail_list.append((one_test_key, one_test_value))

    # 排序成功和失败的任务列表
    pass_list = sorted(pass_list)
    fail_list = sorted(fail_list)
    public_info["pass_num"] = len(pass_list)
    public_info["fail_num"] = len(fail_list)

    # 生成报告公共信息
    public_info_html = gen_report_public_info(public_info)

    # 生成报告详细信息
    detail_info_html = gen_report_detail_info(pass_list, fail_list)

    # 合成报告
    report_info_html = gen_report_info(public_info_html, detail_info_html)

    # 写入文件
    with open(save_file_name, "w", encoding="utf-8") as f:
        f.write(report_info_html)


def add_error_msg_to_public_info(err_msg, report_info):
    """
    将一条错误信息追加到public_info里面的task_error字段中
    err_msg的结构为{"name": "", "error_msg": ""}
    """
    msg = err_msg.get("error_msg")
    if REPORT_PUBLIC_INFO_NAME in report_info:
        public_info = report_info.get(REPORT_PUBLIC_INFO_NAME)
        if TASK_ERROR_KEY_NAME in public_info:
            public_info[TASK_ERROR_KEY_NAME].append(msg)
        else:
            public_info[TASK_ERROR_KEY_NAME] = [msg, ]
    else:
        report_info[REPORT_PUBLIC_INFO_NAME] = {TASK_ERROR_KEY_NAME: [msg, ]}


def add_to_report_info(one_info, report_info):
    """
    将一条信息添加到报告信息集合中，
    不存在则新增，存在则添加或者覆盖以前的值
    one_info格式为 {key: {key1: value ...}, key: {key1: value ...}...}
    当前的key包括如下几类：
    1、任务名称：[sn]_filerecv_0 或者 [ip:port]_filerecv_0
    2、公共信息：REPORT_PUBLIC_INFO_NAME = "public_info"
    3、测试任务报告的错误信息：TASK_ERROR_KEY_NAME = "task_error"
    最终追加到public_info里面的task_error字段中
    """
    for report_section_add, section_dict_add in one_info.items():
        if report_section_add == TASK_ERROR_KEY_NAME:
            # 追加error msg到public_info里面的task_error字段中
            add_error_msg_to_public_info(section_dict_add, report_info)
            continue
        if report_section_add in report_info:
            # 报告数据中已经存在待添加的字段，则获取报告数据中的该段落，给段落中增加或者覆盖已有字段
            report_section_dict = report_info.get(report_section_add)
            report_section_dict.update(section_dict_add)
        else:
            report_info[report_section_add] = section_dict_add


def process_msg(msg_queue, thread_name):
    """
    消息中心，用于接收所有测试线程的消息，保存到如下结构的字典report_info中，用于生成报告：
    {
    "public_info": {"key": value, "key": value, "key": value, "key": value ...}
    "thread_name1": {"key": value, "key": value, "key": value, "key": value ...}
    "thread_name2": {"key": value, "key": value, "key": value, "key": value ...}
    }
    public_info为报告开头的公共信息
    thread_namex为各个测试项目的测试信息
    通过msg_queue，传递过来的消息，格式为 {key: {key1: value ...}, key: {key1: value ...}...}的格式，
    key表示上面report_info的key，不存在则新增，存在则添加或者覆盖以前的值
    """
    logger.info(f"start process_msg thread {thread_name}")
    report_info = {}
    while True:
        one_info = msg_queue.get()
        if one_info is None:  # 报告接收完毕，退出
            logger.info(f"{thread_name} get None from queue")
            break
        add_to_report_info(one_info, report_info)
    logger.info(f"start save report to {REPORT_FILE_NAME}, report len:{len(report_info)}")
    save_report(report_info, REPORT_FILE_NAME)
    logger.info(f"stop process_msg thread {thread_name}")


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
            logger.error(f"start test test_config is empty, do not test, type:{self.session_type} {self.connect_key}")
            return True
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
        task_count = len(config["port_info"])
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
    if len(devices) > 1:
        # 存在多个设备连接，获取不是ip:port的设备作为待测试的设备
        logger.info(f"Multiple devices are connected, devices:{devices}")
        for dev in devices:
            if ':' not in dev:
                device_sn = dev
                # 关闭设备侧监听端口
                result, _ = run_cmd_block(f"hdc -t {device_sn} tmode port close")
                logger.info(f"close tmode port finished")
                time.sleep(10)

    logger.info(f"get device:{device_sn}")

    # 开启设备侧监听端口
    result, _ = run_cmd_block(f"hdc -t {device_sn} tmode port {DEVICE_LISTEN_PORT}")
    logger.info(f"run tmode port {DEVICE_LISTEN_PORT}, result:{result}")
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
    if len(TCP_SESSION_TEST_CONFIG) == 0 or len(port_list) == 0:
        logger.info(f"port_list:{port_list} TCP_SESSION_TEST_CONFIG:{TCP_SESSION_TEST_CONFIG}")
        logger.info(f"port_list or TCP_SESSION_TEST_CONFIG is empty, no need do local tcp session test, exit")
        return True
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


def start_test(msg_queue):
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


def run_stability_test():
    global logger
    logger = logging.getLogger(__name__)
    logger.info(f"main resource_path:{RESOURCE_PATH}")
    start_time = get_time_str()
    if init_test_env() is False:
        logger.error(f"init_test_env failed")
        return False

    # 启动消息收集进程
    msg_thread = threading.Thread(target=process_msg, name="msg_process", args=(TEST_THREAD_MSG_QUEUE, "msg_process"))
    msg_thread.start()

    put_msg(TEST_THREAD_MSG_QUEUE, {"public_info": {"start_time": start_time}})
    logger.info(f"start run test thread")
    if start_test(TEST_THREAD_MSG_QUEUE) is False:
        logger.error(f"start_test failed")
        stop_time = get_time_str()
        put_msg(TEST_THREAD_MSG_QUEUE, {"public_info": {"stop_time": stop_time}})
        TEST_THREAD_MSG_QUEUE.put(None)
        msg_thread.join()
        return False

    # wait all thread exit
    logger.info(f"wait all test thread exit")
    for session in SESSION_LIST:
        for thread in session.thread_list:
            thread.join()

    stop_time = get_time_str()
    put_msg(TEST_THREAD_MSG_QUEUE, {"public_info": {"stop_time": stop_time}})

    release_resource()

    # 传递 None 参数，报告进程收到后退出
    logger.info(f"main put None to TEST_THREAD_MSG_QUEUE")
    TEST_THREAD_MSG_QUEUE.put(None)
    msg_thread.join()

    logger.info(f"exit main, test result:{TEST_RESULT}")
    return TEST_RESULT


if __name__ == "__main__":
    init_logger()
    run_stability_test()
