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
import os
import pytest

from utils import GP, check_hdc_cmd, check_shell, rmdir, get_local_path, get_shell_result, load_gp

MEDIA_PHOTO_DIR = "/mnt/data/100/media_fuse/Photo"
MEDIA_PHOTO_OTHER_DIR = f"{MEDIA_PHOTO_DIR}/其它"
REMOTE_SNAPSHOT_PATH = "/data/local/tmp/test.jpg"


class TestMediaFile:
    local_recv_dir = "其它"
    local_new_dir = "测试"
    local_recv_file = "test_recv.jpg"

    @classmethod
    def setup_class(cls):
        check_hdc_cmd(f"shell rm -rf {REMOTE_SNAPSHOT_PATH}")
        check_hdc_cmd(f"shell rm -rf {MEDIA_PHOTO_OTHER_DIR}")
        check_hdc_cmd(f"shell snapshot_display -f {REMOTE_SNAPSHOT_PATH}")
        check_hdc_cmd(f"shell mkdir -p {MEDIA_PHOTO_OTHER_DIR}")
        check_hdc_cmd(f"shell cp {REMOTE_SNAPSHOT_PATH} {MEDIA_PHOTO_OTHER_DIR}")
        recv_dir_path = get_local_path(cls.local_recv_dir)
        new_dir_path = get_local_path(cls.local_new_dir)
        if not os.path.exists(recv_dir_path):
            os.makedirs(recv_dir_path)
        if not os.path.exists(new_dir_path):
            os.makedirs(new_dir_path)

    @classmethod
    def teardown_class(cls):
        check_hdc_cmd(f"shell rm -rf {REMOTE_SNAPSHOT_PATH}")
        check_hdc_cmd(f"shell rm -rf {MEDIA_PHOTO_OTHER_DIR}")
        recv_dir_path = get_local_path(cls.local_recv_dir)
        new_dir_path = get_local_path(cls.local_new_dir)
        if os.path.exists(recv_dir_path):
            rmdir(recv_dir_path)
        if os.path.exists(new_dir_path):
            rmdir(new_dir_path)

    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_media_file_export(self):
        recv_file = get_local_path(os.path.join(self.local_recv_dir, self.local_recv_file))
        try:
            ls_output = get_shell_result(f"shell ls -l {MEDIA_PHOTO_OTHER_DIR}")
            assert "test" in ls_output
            assert check_hdc_cmd(f"file recv {MEDIA_PHOTO_OTHER_DIR}/test.jpg {recv_file}")
        finally:
            if os.path.exists(recv_file):
                os.remove(recv_file)

    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_media_file_import_merge(self):
        recv_file = get_local_path(os.path.join(self.local_recv_dir, self.local_recv_file))
        recv_dir = get_local_path(self.local_recv_dir)
        try:
            assert check_hdc_cmd(f"file recv {MEDIA_PHOTO_OTHER_DIR}/test.jpg {recv_file}")
            assert check_hdc_cmd(f"file send {recv_dir} {MEDIA_PHOTO_DIR}")
        finally:
            if os.path.exists(recv_file):
                os.remove(recv_file)

    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_media_file_import_same_dir_merge(self):
        recv_file = get_local_path(os.path.join(self.local_recv_dir, self.local_recv_file))
        recv_dir = get_local_path(self.local_recv_dir)
        try:
            assert check_hdc_cmd(f"file recv {MEDIA_PHOTO_OTHER_DIR}/test.jpg {recv_file}")
            assert check_hdc_cmd(f"file send {recv_dir} {MEDIA_PHOTO_DIR}")
            ls_output = get_shell_result(f"shell ls {MEDIA_PHOTO_OTHER_DIR}")
            assert "test.jpg" in ls_output
        finally:
            if os.path.exists(recv_file):
                os.remove(recv_file)

    @pytest.mark.L0
    @pytest.mark.repeat(1)
    def test_media_file_import_new_dir_fail(self):
        send_dir = get_local_path(self.local_new_dir)
        assert check_shell(f"file send {send_dir} {MEDIA_PHOTO_DIR}", "Error create directory")
