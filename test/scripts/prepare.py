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

import subprocess
import zipfile
import hashlib
import os


def install_dependencies(requirements_file):
    try:
        subprocess.check_call(["pip", "install", "-r", requirements_file])
        print(f"install requirements.txt success")
    except subprocess.CalledProcessError as e:
        print(f"install dependence fail: {str(e)}")
 

def check_exist_file():
    file_list = []
    file_list.append("AACommand07.hap")
    file_list.append("libA_v10001.hsp")
    file_list.append("libB_v10001.hsp")
    url = "https://gitee.com/TaowerfulMAX/h2dij432sfa423o_debugfortest/releases/download/0.0.1-debug/package_0.0.1.zip"
    expected_md5 = "ace0655b5f8dfb181544c92ae90f7bfc"
    from testModule.utils import GP
    for file in file_list:
        if not os.path.exists(os.path.join(GP.local_path, file)):
            if download_and_extract_zip(url, os.path.join(GP.local_path), expected_md5):
                print(f"{file} File Download Success")
                continue
            else:
                print(f"{file} File Download Failed")
            print(f"No {file} File!")
            print(f"请自行访问以下链接，下载package.zip中的安装包文件解压到当前脚本resource目录中，"
                  "操作完成该步骤后重新执行prepare.py脚本。")
            print("Please download from the url below and unzip package.zip to resource directory,"
                  "please rerun after operation.")
            print("url: " + url)
            exit(1)


def download_file(url, timeout=(10, 30)):
    try:
        import requests
    except ModuleNotFoundError:
        print("Please install requests module, command: [pip install requests]")
        exit(1)
    try:
        response = requests.get(url, timeout=timeout)
        response.raise_for_status()  # 将触发HTTPError，如果状态不是200
        return True, response.content
    except requests.exceptions.Timeout:
        return False, "请求超时"
    except requests.exceptions.HTTPError as err:
        return False, f"HTTP错误：{err}"
    except requests.exceptions.RequestException as e:
        return False, f"请求异常：{e}"


def save_to_file(content, filename):
    with open(filename, 'wb') as f:
        f.write(content)


def extract_zip(filename, extract_to='.'):
    with zipfile.ZipFile(filename, 'r') as zip_ref:
        zip_ref.extractall(extract_to)


def calculate_md5(filename):
    hash_md5 = hashlib.md5()
    try:
        with open(filename, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        return hash_md5.hexdigest()
    except PermissionError:
        return "PermissionError"
    except FileNotFoundError:
        return "FileNotFoundError"


def download_and_extract_zip(url, extract_to='.', expected_md5=None):
    # 下载ZIP文件
    is_success, content = download_file(url)
    if not is_success:
        print(f"download_file failed: {content}")
        return False
    # 获取ZIP文件名
    zip_filename = url.split('/')[-1]
    
    # 写入本地文件
    save_to_file(content, zip_filename)
    
    # 如果提供了预期的MD5值，则进行校验
    if expected_md5:
        file_md5 = calculate_md5(zip_filename)
        if file_md5 != expected_md5:
            raise Exception(f"MD5校验失败：预期的MD5为{expected_md5}，实际为{file_md5}")
        else:
            print("MD5校验成功")
    
    # 解压ZIP文件
    extract_zip(zip_filename, extract_to)
    
    # 可选：删除ZIP文件
    os.remove(zip_filename)
    print(f"文件已解压到：{extract_to}")
    return True


def prepare():
    install_dependencies("requirements.txt")
    from testModule.utils import GP, gen_package_dir, update_source, prepare_source
    test_path = os.path.join(os.getcwd(), "testModule")
    if not os.path.exists(test_path):
        print("testModule not exist")
        return
    GP.init()
    prepare_source()
    update_source()
    check_exist_file()
    gen_package_dir()


if __name__ == "__main__":
    prepare()