#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Huawei Device Co., Ltd.
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
import re
import subprocess
import tempfile
import time
import multiprocessing
import logging
import pytest

from utils import GP, check_version, get_shell_result


class TestHdcdSecurity:
    @classmethod
    def setup_class(self):
        pass

    @classmethod
    def teardown_class(self):
        pass

    @pytest.mark.L0
    @check_version("Ver: 3.1.0a")
    def test_check_include_cpp(self):
        product = get_shell_result(f'shell param get const.product.software.version')
        cppstr = get_shell_result(f'shell strings $(which hdcd) | grep cpp')
        if 'OpenHarmony' in product:
            assert cppstr != ''
        else:
            assert cppstr == ''

    @pytest.mark.L0
    @check_version("Ver: 3.1.0a")
    def test_check_source_file_leak(self):
        """校验 hdcd 编译产物中是否泄漏源码文件名和路径

        OpenHarmony 开源版：WRITE_LOG 使用 __FILE_NAME__，二进制中应有源文件名
        商业发布版（IS_RELEASE_VERSION）：WRITE_LOG 使用 __FUNCTION__，二进制中不应有源文件名
        """
        # NUL 锚定：只在 ELF 字符串表的真实字符串内匹配源码文件名，杜绝 "cpp" 随机子串误报
        source_file_pattern = re.compile(rb'[\w./+-]+\.(?:cpp|cc|cxx|h|hpp)\x00')

        product = get_shell_result('shell param get const.product.software.version')
        hdcd_path = get_shell_result('shell which hdcd').strip()
        assert hdcd_path, 'hdcd not found on device'

        with tempfile.TemporaryDirectory() as tmp:
            local = os.path.join(tmp, 'hdcd')
            subprocess.check_call(f'{GP.hdc_head} file recv {hdcd_path} {local}'.split())
            with open(local, 'rb') as f:
                data = f.read()

        matches = sorted(set(m.decode().rstrip('\x00') for m in source_file_pattern.findall(data)))

        if 'OpenHarmony' in product:
            assert matches, 'hdcd should embed source file names (open-source build)'
        else:
            assert not matches, f'hdcd leaked source file names: {matches}'

