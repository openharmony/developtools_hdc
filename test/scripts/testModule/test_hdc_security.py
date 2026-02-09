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

