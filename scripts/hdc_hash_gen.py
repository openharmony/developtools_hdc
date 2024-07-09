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

import os
import sys
import argparse
import hashlib

ALGORITHM = None
HDC_FILE = ""
INPUT_DIR = ""
OUTPUT_DIR = ""
struct_vals = []
CFG_FILE_NAME = r"scripts/file_path.cfg"
OUTPUT_FILE_NAME = r"all.txt"


def calc_file_hash():
    if OUTPUT_DIR == "":
        return
    global ALGORITHM
    ALGORITHM = hashlib.sha256()
    size = os.path.getsize("{}{}".format(OUTPUT_DIR, OUTPUT_FILE_NAME))
    with open("{}{}".format(OUTPUT_DIR, OUTPUT_FILE_NAME), 'rb') as fd:
        while size >= 1024 * 1024:
            ALGORITHM.update(fd.read(1024 * 1024))
            size -= 1024 * 1024
        ALGORITHM.update(fd.read())


def write_output_file():
    if OUTPUT_DIR == "":
        return
    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR, exist_ok=True)
    with open("{}{}".format(OUTPUT_DIR, OUTPUT_FILE_NAME), 'w') as fd_struct:
        for i in struct_vals:
            fd_struct.write(i)
            fd_struct.write('\n')


def write_hdc_file():
    if HDC_FILE == "":
        return
    with open("{}{}".format(OUTPUT_DIR, HDC_FILE), 'w') as fd_hdc:
        fd_hdc.write("#ifndef HDC_HASH_GEN_H\n")
        fd_hdc.write("#define HDC_HASH_GEN_H\n")
        fd_hdc.write('\n')
        fd_hdc.write("#include <stdio.h>\n")
        context = "{}{}{}".format("#define HDC_MSG_HASH \"", str(ALGORITHM.hexdigest())[0:16], "\"")
        fd_hdc.write(context)
        fd_hdc.write("\n\n")
        fd_hdc.write("#endif\n")


def read_struct():
    if INPUT_DIR == "":
        return
    with open("{}/{}".format(INPUT_DIR, CFG_FILE_NAME), mode='r', encoding='utf-8') as fd_path:
        for line in fd_path.readlines():
            file_name = line.strip()
            with open("{}{}".format(INPUT_DIR, file_name), mode='r', encoding='utf-8') as fd_file:
                is_find = False
                is_end = False
                begin_count = 0
                end_count = 0
                for file_line in fd_file.readlines():
                    context = file_line.strip()
                    if is_find and not is_end:
                        struct_vals.append(context)
                        if context.find("{") != -1:
                            begin_count = begin_count + 1
                        if context.find("}") != -1:
                            end_count = end_count + 1
                        if begin_count == end_count and begin_count != 0:
                            is_end = True
                            begin_count = 0
                            end_count = 0
                    if context.find("struct") != -1:
                        is_find = True
                        is_end = False
                        struct_vals.append(context)
                        if context.find("{") != -1:
                            begin_count = begin_count + 1


def main():
    parser = argparse.ArgumentParser(
        description='Hdc proto code generator.')
    parser.add_argument('-f', dest='HDC_FILE', required=True, type=str,
                        help='output file name')
    parser.add_argument('-i', dest='INPUT_DIR', required=True, type=str,
                        help='input directory')
    parser.add_argument('-o', dest='OUTPUT_DIR', required=True, type=str,
                        help='output directory')

    args = parser.parse_args(sys.argv[1:])
    global HDC_FILE
    HDC_FILE = args.HDC_FILE
    print("HDC_FILE:", HDC_FILE)
    global INPUT_DIR
    INPUT_DIR = args.INPUT_DIR
    print("INPUT_DIR:", INPUT_DIR)
    global OUTPUT_DIR
    OUTPUT_DIR = args.OUTPUT_DIR
    print("OUTPUT_DIR:", OUTPUT_DIR)


if __name__ == '__main__':
    print("~~~~~~~~~~~~~~~~ hdc_hash begin ~~~~~~~~~~~~~~~~~~")
    main()
    read_struct()
    write_output_file()
    calc_file_hash()
    write_hdc_file()