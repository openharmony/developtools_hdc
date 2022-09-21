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

algorithm = None
hdc_file = ""
input_dir = ""
output_dir = ""
struct_vals = []
cfg_file_name = r"/scripts/file_path.cfg"
output_file_name = r"all.txt"

def calc_file_hash():
    if output_dir == "":
        return
    global algorithm
    algorithm = hashlib.sha256()
    size = os.path.getsize("{}{}".format(output_dir, output_file_name))
    with open("{}{}".format(output_dir, output_file_name), 'rb') as fd:
        while size >= 1024 * 1024:
            algorithm.update(fd.read(1024 * 1024))
            size -= 1024 * 1024
        algorithm.update(fd.read())

def write_output_file():
    if output_dir == "":
        return
    with open("{}{}".format(output_dir, output_file_name), 'w') as fd_struct:
        for i in struct_vals:
            fd_struct.write(i)
            fd_struct.write('\n')

def write_hdc_file():
    if hdc_file == "":
        return
    with open("{}{}".format(output_dir, hdc_file), 'w') as fd_hdc:
        fd_hdc.write("#ifndef HDC_HASH_GEN_H\n")
        fd_hdc.write("#define HDC_HASH_GEN_H\n")
        fd_hdc.write('\n')
        fd_hdc.write("#include <stdio.h>\n")
        context = "{}{}{}".format("#define HDC_MSG_HASH \"", str(algorithm.hexdigest())[0:16], "\"")
        fd_hdc.write(context)
        fd_hdc.write("\n\n")
        fd_hdc.write("#endif\n")

def read_struct():
    if input_dir == "":
        return
    with open("{}{}".format(input_dir , cfg_file_name), mode='r', encoding='utf-8') as fd_path:
        for line in fd_path.readlines():
            file_name = line.strip()
            with open("{}{}".format(input_dir , file_name), mode='r', encoding='utf-8') as fd_file:
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
    parser.add_argument('-f', dest='hdc_file', required=True, type=str,
                        help='output file name')
    parser.add_argument('-i', dest='input_dir', required=True, type=str,
                        help='input directory')
    parser.add_argument('-o', dest='output_dir', required=True, type=str,
                        help='output directory')

    args = parser.parse_args(sys.argv[1:])
    global hdc_file
    hdc_file = args.hdc_file
    print("hdc_file:", hdc_file)
    global input_dir
    input_dir = args.input_dir
    print("input_dir:", input_dir)
    global output_dir
    output_dir = args.output_dir
    print("output_dir:", output_dir)

if __name__ == '__main__':
    print("~~~~~~~~~~~~~~~~ hdc_hash begin ~~~~~~~~~~~~~~~~~~")
    main()
    read_struct()
    write_output_file()
    calc_file_hash()
    write_hdc_file()