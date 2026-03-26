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
import pytest
from utils import run_command_with_timeout


class TestSudoCmd:
    @classmethod
    def setup_class(self):
        pass

    @classmethod
    def teardown_class(self):
        pass

    @pytest.mark.L0
    def test_sudo_id(self):
        cmd = 'sudo id'
        output, _ = run_command_with_timeout(cmd, 3)
        output = output.replace('\n', '').replace('\r', '')
        output_str = 'uid=0(root) gid=0(root) groups=0(root),1006(file_manager),1007(log),1097(netsys_socket),3009(readproc),3009(shader_cache) context=u:r:sudo_execv_label:s0'
        assert output == output_str


    @pytest.mark.L0
    def test_sudo_sh_c_id(self):
        cmd = 'sudo sh c id'
        output, _  = run_command_with_timeout(cmd, 3)
        output = output.replace('\n', '').replace('\r', '')
        output_str = 'uid=0(root) gid=0(root) groups=0(root),1006(file_manager),1007(log),1097(netsys_socket),3009(readproc),3009(shader_cache) context=u:r:sudo_execv_label:s0'
        assert output == output_str


    @pytest.mark.L0
    def test_sudo_echo_id(self):
        cmd = 'sudo echo 1'
        output, error = run_command_with_timeout(cmd, 3)
        output = output.replace('\n', '').replace('\r', '')
        error = error.replace('\n', '').replace('\r', '')
        output_str = '1'
        assert output == output_str and error == ''


    @pytest.mark.L0
    def test_sudo_null(self):
        cmd = 'sudo ""'
        output, error = run_command_with_timeout(cmd, 3)
        output = output.replace('\n', '').replace('\r', '')
        error = error.replace('\n', '').replace('\r', '')
        output_str = '[E0002] command not found'
        assert output == output_str and error == ''


    @pytest.mark.L0
    def test_sudo_noparam(self):
        cmd = 'sudo'
        output, error = run_command_with_timeout(cmd, 3)
        output = output.replace('\n', '').replace('\r', '')
        error = error.replace('\n', '').replace('\r', '')
        error_str = 'sudo - execute command or script as rootusage: sudo command ...'
        assert error == error_str and output == ''


    @pytest.mark.L0
    def test_sudo_cmd_long(self):
        cmd1 = 'sudo aaaaaa...'
        output1, error1 = run_command_with_timeout(cmd1, 3)
        output1 = output1.replace('\n', '').replace('\r', '')
        error1 = error1.replace('\n', '').replace('\r', '')

        assert error1 == '' and output1 == ''

        cmd2 = 'sudo aaaaa...'
        output2, error2 = run_command_with_timeout(cmd2, 3)
        output2 = output2.replace('\n', '').replace('\r', '')
        error2 = error2.replace('\n', '').replace('\r', '')

        assert error2 == '' and output2 == ''


    @pytest.mark.L0
    def test_sudo_long_param(self):
        cmd1 = 'sudo echo aaaaa...'
        output, error = run_command_with_timeout(cmd1, 3)
        output = output.replace('\n', '').replace('\r', '')
        error = error.replace('\n', '').replace('\r', '')

        assert error == '' and output == 'aaaaa...'


    @pytest.mark.L0
    def test_sudo_env_clear_allmatch(self):
        cmd1 = 'sh -c "export CAPATH=xxxxxxxxxxxxxxxxxx"'
        output1, _ = run_command_with_timeout(cmd1, 3)
        output1 = output1.replace('\n', '').replace('\r', '')
        assert output1 == ''

        cmd2 = 'sudo sh ./test.sh'
        output2, _ = run_command_with_timeout(cmd2, 3)
        output2 = output2.replace('\n', '').replace('\r', '')
        assert output2 == ''

    
    @pytest.mark.L0
    def test_sudo_env_clear_wildcard(self):
        cmd1 = 'sh -c "export _RLDxxx=xxxxxxxxxxxxxxxxxx"'
        cmd2 = 'sh -c "export _RLD=xxxxxxxxxxxxxxxxxx"'
        output1, _ = run_command_with_timeout(cmd1, 3)
        output2, _ = run_command_with_timeout(cmd2, 3)
        output1 = output1.replace('\n', '').replace('\r', '')
        output2 = output2.replace('\n', '').replace('\r', '')
        assert output1 == ''
        assert output2 == ''

        cmd3 = 'sudo sh ./test.sh'
        output3, _ = run_command_with_timeout(cmd3, 3)
        output3 = output3.replace('\n', '').replace('\r', '')
        assert output3 == ''


    @pytest.mark.L0
    def test_sudo_nosafe_env_check_allmatch(self):
        cmd1 = 'sh -c "export COLORTERM=xxxxxxxx/xxxx%xxxxx"'
        output1, _ = run_command_with_timeout(cmd1, 3)
        output1 = output1.replace('\n', '').replace('\r', '')
        assert output1 == ''

        cmd2 = 'sudo sh ./test.sh'
        output2, _ = run_command_with_timeout(cmd2, 3)
        output2 = output2.replace('\n', '').replace('\r', '')
        assert output2 == ''
    
    
    @pytest.mark.L0
    def test_sudo_nosafe_env_check_wildcard(self):
        cmd1 = 'sh -c "export LC_=xxxxxxxx/xxxx%xxxxx"'
        cmd2 = 'sh -c "export LC_xxx=xxxxxxxx/xxxx%xxxxx"'
        output1, _ = run_command_with_timeout(cmd1, 3)
        output2, _ = run_command_with_timeout(cmd2, 3)
        output1 = output1.replace('\n', '').replace('\r', '')
        output2 = output2.replace('\n', '').replace('\r', '')
        assert output1 == ''
        assert output2 == ''

        cmd3 = 'sudo sh ./test.sh'
        output3, _ = run_command_with_timeout(cmd3, 3)
        output3 = output3.replace('\n', '').replace('\r', '')
        assert output3 == ''


    @pytest.mark.L0
    def test_sudo_noncompliant_tz_env_check(self):
        cmd1 = 'sh -c "export TZ=x/../xxxxxxxxxxxxxxxx"'
        output1, _ = run_command_with_timeout(cmd1, 3)
        output1 = output1.replace('\n', '').replace('\r', '')
        assert output1 == ''

        cmd2 = 'sudo sh ./test.sh'
        output2, _ = run_command_with_timeout(cmd2, 3)
        output2 = output2.replace('\n', '').replace('\r', '')
        assert output2 == ''    


    @pytest.mark.L0
    def test_sudo_safe_env_check_allmatch(self):
        with open('./test.sh', 'w') as f:
            f.write('#!/bin/bash\nenv | grep xxxxxx\n')
        os.chmod('./test.sh', 0o755)

        cmd1 = 'sudo sh -c "export COLORTERM=xxxxxxxxxxxxxxxxxx"'
        output1, _ = run_command_with_timeout(cmd1, 3)
        output1 = output1.replace('\n', '').replace('\r', '')
        assert output1 == ''

        cmd2 = 'sudo sh ./test.sh'
        output2, _ = run_command_with_timeout(cmd2, 3)
        output2 = output2.replace('\n', '').replace('\r', '')
        assert output2 == 'COLORTERM=xxxxxxxxxxxxxxxxxx'


    @pytest.mark.L0
    def test_sudo_safe_env_check_wildcard(self):
        with open('./test.sh', 'w') as f:
            f.write('#!/bin/bash\nenv | grep xxxxxx\n')
        os.chmod('./test.sh', 0o755)

        cmd1 = 'sudo sh -c "export LC_=xxxxxxxxxxxxxxxxxx"'
        cmd2 = 'sudo sh -c "export LC_xxx=xxxxxxxxxxxxxxxxxx"'
        output1, _ = run_command_with_timeout(cmd1, 3)
        output2, _ = run_command_with_timeout(cmd2, 3)
        output1 = output1.replace('\n', '').replace('\r', '')
        output2 = output2.replace('\n', '').replace('\r', '')
        assert output1 == ''
        assert output2 == ''

        cmd3 = 'sudo sh ./test.sh'
        output3, _ = run_command_with_timeout(cmd3, 3)
        output3 = output3.replace('\n', '').replace('\r', '')
        assert output3 == 'LC_=xxxxxxxxxxxxxxxxxxLC_xxx=xxxxxxxxxxxxxxxxxx'


    @pytest.mark.L0
    def test_sudo_compliant_tz_env_check(self):
        with open('./test.sh', 'w') as f:
            f.write('#!/bin/bash\nenv | grep xxxxxx\n')
        os.chmod('./test.sh', 0o755)

        cmd1 = 'sh -c "export TZ=xxxxxxxxxxxxxxxxxx"'
        output1, _ = run_command_with_timeout(cmd1, 3)
        output1 = output1.replace('\n', '').replace('\r', '')
        assert output1 == ''

        cmd2 = 'sudo sh ./test.sh'
        output2, _ = run_command_with_timeout(cmd2, 3)
        output2 = output2.replace('\n', '').replace('\r', '')
        assert output2 == 'TZ=xxxxxxxxxxxxxxxxxxx'  