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
import logging
import pytest
import subprocess

from utils import GP, check_hdc_cmd, check_shell, check_version, get_shell_result, run_command_with_timeout, load_gp, \
    get_hdcd_pss, get_end_symbol

logger = logging.getLogger(__name__)


class TestUpdaterMode:
    """Test updater mode functionality"""

    pss = 0

    @classmethod
    def setup_class(cls):
        """Setup updater mode for all tests in this class"""
        logger.info("Entering updater mode: hdc shell reboot updater")
        check_shell("shell reboot updater")
        time.sleep(5)
        
        logger.info("Waiting for device: hdc wait")
        run_command_with_timeout(f"{GP.hdc_head} wait", 30)
        time.sleep(3)

    @classmethod
    def teardown_class(cls):
        """Exit updater mode after all tests in this class"""
        logger.info("Exiting updater mode: hdc shell reboot")
        check_shell("shell reboot")
        time.sleep(5)
        
        logger.info("Waiting for device: hdc wait")
        run_command_with_timeout(f"{GP.hdc_head} wait", 30)
        time.sleep(3)

    @pytest.mark.L0
    def test_shell_check_updater(self):
        """Test shell is in updater mode"""
        result = check_shell("shell param get updater.hdc.configfs", "1")
        assert result

    @pytest.mark.L0
    def test_shell_pwd_command(self):
        """Test shell pwd command in updater mode"""
        result = check_shell("shell pwd")
        assert result

    @pytest.mark.L0
    def test_shell_ls_command(self):
        """Test shell ls command in updater mode"""
        result = check_shell("shell ls /")
        assert result

    @pytest.mark.L0
    def test_shell_whoami_command(self):
        """Test shell whoami command in updater mode"""
        result = check_shell("shell whoami")
        assert result

    @pytest.mark.L0
    def test_shell_uname_command(self):
        """Test shell uname command in updater mode"""
        result = check_shell("shell uname -a")
        assert result

    @pytest.mark.L0
    def test_system_partition_access(self):
        """Test system partition access in updater mode"""
        result = check_shell("shell ls /system")
        assert result

    @pytest.mark.L0
    def test_vendor_partition_access(self):
        """Test vendor partition access in updater mode"""
        result = check_shell("shell ls /vendor")
        assert result

    @pytest.mark.L0
    def test_proc_access(self):
        """Test /proc/ access in updater mode"""
        result = check_shell("shell ls /proc")
        assert result
        
        result = check_shell("shell cat /proc/version")
        assert result

    @pytest.mark.L0
    def test_sys_access(self):
        """Test /sys access in updater mode"""
        result = check_shell("shell ls /sys")
        assert result

    @pytest.mark.L0
    def test_file_read_operations(self):
        """Test file read operations in updater mode"""
        result = check_shell("shell cat /proc/cmdline")
        assert result
        
        result = check_shell("shell cat /proc/version")
        assert result

    @pytest.mark.L0
    def test_file_write_operations(self):
        """Test file write operations in updater mode"""
        test_file = "/data/local/tmp/updater_test.txt"
        test_content = "updater mode test content"
        check_shell("shell mkdir /data/local")
        check_shell("shell mkdir /data/local/tmp")
        check_shell(f"shell touch {test_file}")
        check_shell(f"shell echo '{test_content}' > {test_file}")
        result = check_shell(f"shell cat {test_file}", test_content)
        assert result
        
        check_shell(f"shell rm {test_file}")

    @pytest.mark.L0
    def test_file_permissions(self):
        """Test file permissions in updater mode"""
        test_file = "/data/local/tmp/updater_perms.txt"
        
        check_shell(f"shell touch {test_file}")
        check_shell(f"shell chmod 755 {test_file}")
        result = check_shell(f"shell ls -l {test_file}")
        assert result
        
        check_shell(f"shell rm {test_file}")

    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    def test_updater_mode_bundle_path_constant(self):
        """Test that DEBUG_BUNDLE_PATH is correct in updater mode"""
        result = check_shell("shell ls /mnt/debug/100/debug_hap/")
        assert result

    @pytest.mark.L0
    @check_version("Ver: 3.1.0e")
    def test_updater_mode_without_os_account(self):
        """Test functionality works without os_account module"""
        result = check_shell("shell pwd")
        assert result
        
        result = check_shell("shell ls /data")
        assert result

    @pytest.mark.L0
    def test_invalid_command_handling(self):
        """Test invalid commands are handled properly in updater mode"""
        result = check_shell("shell invalid_command_xyz", "/bin/sh: invalid_command_xyz: inaccessible or not found")
        assert result

    @pytest.mark.L0
    def test_invalid_path_handling(self):
        """Test invalid paths are handled properly in updater mode"""
        result = check_shell("shell ls /nonexistent/path/xyz", "ls: /nonexistent/path/xyz: No such file or directory")
        assert result

    @pytest.mark.L0
    def test_command_response_time(self):
        """Test command response time in updater mode"""
        start_time = time.time()
        result = check_shell("shell pwd")
        end_time = time.time()
        
        assert result
        assert (end_time - start_time) < 10.0

    @pytest.mark.L0
    def test_multiple_commands_performance(self):
        """Test performance of multiple commands"""
        start_time = time.time()
        
        for i in range(5):
            check_shell("shell pwd")
            check_shell("shell ls /")
        
        end_time = time.time()
        
        assert (end_time - start_time) < 30.0

    @pytest.mark.L0
    def test_after_error_recovery(self):
        """Test recovery after error"""
        check_shell("shell invalid_command_xyz")
        
        result = check_shell("shell pwd")
        assert result

    @pytest.mark.L0
    def test_server_restart_recovery(self):
        """Test recovery after server restart"""
        result = check_hdc_cmd("start")
        assert result
        
        time.sleep(2)
        
        result = check_shell("shell pwd")
        assert result
