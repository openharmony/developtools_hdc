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
# 运行环境: python 3.10+, tqdm
# pip install tqdm
# python hdc_recv_all_test.py

import datetime
import subprocess
import os
import stat
import logging
from multiprocessing import Pool
import tqdm

cwd = os.getcwd()
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')


def get_files_list(args=""):
    """
    This function returns a list of files for the input args.
    """

    if args == "":
        cmd = f"hdc shell find"
    else:
        cmd = f"hdc shell find {args}"
    output = subprocess.check_output(cmd.split()).decode()
    return output.split('\r\n')[:-1]


def ls_z_check(path):
    """
    List all files in directory with selinux context.
    """

    cmd = f"hdc shell \"ls -alZ {path}\""
    output = subprocess.check_output(cmd.split()).decode()
    return output.replace('\r\n', ' ').replace(',', ' ').replace('\r', ' ')


def file_copy_check(local, remote, file_id):
    """
    Check if the file has been successfully copied and could be transferred by hdc.
    """

    if file_id >= 0:
        pass
    else:
        return f"file_id error"
    check_ret = False
    recv_ret = False
    copy_output = ""
    check_output = ""
    recv_output = ""
    remove_output = ""
    list_z_check = ls_z_check(remote)

    if "ls:" not in list_z_check:
        check_ret = True
    local_real = os.path.join(local, str(file_id))
    if local != "":
        if os.path.exists(local):
            pass
        else:
            os.mkdir(local)
        device_tmp_dir = "data/local/tmp"
        copy_cmd = f"hdc shell mkdir -p {device_tmp_dir}/{file_id} && cp -rf {remote} {device_tmp_dir}/{file_id}/"
        shell_cmd = f"hdc shell ls -alZ {device_tmp_dir}/{file_id}/"
        recv_cmd = f"hdc file recv {device_tmp_dir}/{file_id} {local}"
        remove_cmd = f"hdc shell rm -rf {device_tmp_dir}/{file_id}"
        copy_output = subprocess.check_output(copy_cmd.split()).decode().replace(',', ' ')
        check_output = subprocess.check_output(shell_cmd.split()).decode().replace(',', ' ')
        recv_output = subprocess.check_output(recv_cmd.split()).decode().replace(',', ' ')
        remove_output = subprocess.check_output(remove_cmd.split()).decode().replace(',', ' ')
        if os.path.exists(local_real):
            recv_ret = True
    out_string = f"{check_ret},{recv_ret},{remote},{local}\
        {list_z_check},{copy_output},{check_output},{recv_output},{remove_output}"
    return out_string


def file_recv_check(local, remote, file_id):
    """
    Check if the file could be transferred by hdc directly.    
    """

    if file_id >= 0:
        pass
    else:
        return f"file_id error"
    check_ret = False
    recv_ret = False
    recv_output = ""

    list_z_check = ls_z_check(remote)
    local_real = os.path.join(local, str(file_id))
    if "ls:" not in list_z_check:
        check_ret = True
    if local != "":
        if os.path.exists(local):
            pass
        else:
            os.mkdir(local)
        recv_cmd = f"hdc file recv {remote} {local_real}"
        recv_output = run_command_with_timeout(recv_cmd, timeout=60)[0].replace(',', ' ')
        if "FileTransfer finish" in recv_output:
            recv_ret = True
        elif os.path.exists(local_real):
            recv_ret = True
        out_string = f"{check_ret},{recv_ret},{remote},{local},{list_z_check},{recv_output}"
    return out_string


def run_command_with_timeout(command, timeout):
    """
    Run a command with timeout.
    """

    try:
        result = subprocess.run(command, check=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=timeout)
        return result.stdout.decode(), result.stderr.decode()
    except subprocess.TimeoutExpired:
        return "", "Command timed out"
    except subprocess.CalledProcessError as e:
        return "", e.stderr.decode()
    

def write_list_csv(device_path="", host_tmp_dir=""):
    """
    Write the output of the file check result to a csv file.
    """

    list_file = get_files_list(device_path)
    save_file = os.path.join(host_tmp_dir, "files_list.csv")
    flags = os.O_WRONLY | os.O_CREAT
    modes = stat.S_IRUSR | stat.S_IWUSR
    with os.fdopen(os.open(save_file, flags, modes), 'w') as fd:
        for file_name in list_file:
            if file_name != "":
                fd.write(f"{file_name}\n")
    logging.info("List of files written to %s", save_file)


def process_files(tmp_dir, files, process_id, chunk_size, action):
    """
    Process files in chunks.
    """

    save_dir = os.path.join(tmp_dir, f"recv_{process_id}_dir")
    save_csv = os.path.join(tmp_dir, f"recv_files_list_{process_id}.csv")
    if not os.path.exists(save_dir):
        os.mkdir(save_dir)
    cnt = 0
    flags = os.O_WRONLY | os.O_CREAT
    modes = stat.S_IRUSR | stat.S_IWUSR
    with os.fdopen(os.open(save_csv, flags, modes), 'w') as fd:
        for file in tqdm.tqdm(files):
            file_name = file.strip()
            cnt += 1
            file_id = process_id * chunk_size + cnt
            if action == 'recv':
                out_string = file_recv_check(save_dir, file_name, file_id)
            elif action == 'copy':
                out_string = file_copy_check(save_dir, file_name, file_id)
            else:
                out_string = ""
            data = f"{str(file_id)},{out_string}".replace('\r\n', ' ').replace('\r', ' ')
            fd.write(f"{data}\n")


def split_files(file_name, split_num):
    """
    Multiprocess split file input list into chunks.
    """

    if not os.path.exists(file_name):
        logging.error("File %s not found", file_name)
        return []
    if split_num <= 0:
        logging.error("Split number must be greater than 0")
        return []
    all_list = []
    with open(file_name, 'r') as fd:
        lines = fd.readlines()
        size = len(lines)
        chunk_size = round(size / split_num)
        for i in range(split_num):
            start = i * chunk_size
            end = start + chunk_size
            if i == split_num - 1:
                end = size
            table_list = lines[start:end]
            all_list.append(table_list)
    return all_list


def merge_file(tmp_dir, file_num, input_name, action='recv'):
    """
    Merge ouput csv into single file.
    """

    files = []
    header = ""
    for i in range(file_num):
        file = os.path.join(tmp_dir, f"{input_name}_{i}.csv")
        files.append(file)
    new_file = os.path.join(tmp_dir, f"merge_dir.csv")
    if action == 'recv':
        header = "file_id,ls result,recv result,full path,save dir,ls output,recv output\n"
    elif action == 'copy':
        header = "file_id,ls result,recv result,full path,dave dir,ls output,copy output,copied ls output,\
            recv output,remove output\n"
    flags = os.O_WRONLY | os.O_CREAT
    modes = stat.S_IRUSR | stat.S_IWUSR
    with os.fdopen(os.open(new_file, flags, modes), 'w') as fd:
        fd.write(header)
        for file in files:
            readline_merge(file, fd)


def readline_merge(input_file, output_fd):
    """
    Read file line by line and write to output file.
    """

    with open(input_file, 'r') as f:
        lines = f.readlines()
        for line in lines:
            output_fd.write(line)


def run_file_check_all(tmp_dir, subprocess_num, action):
    """
    Run file check, for all the files.
    """

    file_name = os.path.join(tmp_dir, "files_list.csv")
    all_list = split_files(file_name, subprocess_num)
    results = []
    input_name = "recv_files_list"
    with Pool(processes=subprocess_num) as pool:
        for i in range(subprocess_num):
            result = pool.apply_async(process_files, args=(tmp_dir, all_list[i], i, len(all_list[0]), action))
            results.append(result)
        
        pool.close()
        pool.join()
        merge_file(tmp_dir, subprocess_num, input_name, action)


def main():
    """
    Main function.
    """

    time_stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d_%H")
    tmp_dir = os.path.join(cwd, time_stamp)
    save_file = os.path.join(tmp_dir, "files_list.csv")
    if not os.path.exists(save_file):
        input("第一次执行请使用root版本，\n请按下[ENTER]确认开始收集")
        if not os.path.exists(tmp_dir): 
            os.mkdir(tmp_dir)
        args = "/ -type d 2>/dev/null"
        write_list_csv(args, tmp_dir)
    else:
        subprocess_num = 14
        num = input("第二次执行请使用user版本，\n请确保第一次执行和第二次执行处在同一小时\n"\
            "请输入数字选择测试内容：\n1. [recv]user版本文件直接收取\n2. [copy]user版本文件在cp后间接收取\n")
        action = ""
        if num == "1":
            action = 'recv'
        elif num == "2":
            action = 'copy'
        else:
            logging.error("输入错误,测试程序已退出，请重新执行")
            return
        run_file_check_all(tmp_dir, subprocess_num, action)


if __name__ == "__main__":
    main()