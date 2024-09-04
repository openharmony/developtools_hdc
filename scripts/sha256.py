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
import hashlib
import os.path

if __name__ == '__main__':
    home_dir = os.path.expanduser("~")
    hdckey_path = os.path.join(home_dir, ".harmony", "hdckey.pub")

    if os.path.exists(hdckey_path):
        with open(hdckey_path, "r") as f:
            hdckey_content = f.read()
            sha256 = hashlib.sha256()
            sha256.update(hdckey_content.encode())
            result = sha256.hexdigest()
            upper = result.upper()
            final = ':'.join([upper[i : i + 2] for i in range(0, len(upper), 2)])
            print(final)
    else:
        print("hdckey.pub 文件不存在")