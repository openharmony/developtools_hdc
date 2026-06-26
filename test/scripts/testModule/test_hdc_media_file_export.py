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

from utils import check_hdc_cmd, rmdir, get_local_path, get_shell_result


MEDIA_PHOTO_DIR = "/data/local/tmp"
MEDIA_PHOTO_OTHER_DIR = f"{MEDIA_PHOTO_DIR}/Other"
REMOTE_SNAPSHOT_PATH = "/data/local/tmp/test.jpeg"


class TestMediaFile:
    local_recv_dir = "Other"
    local_new_dir = "Test"
    local_recv_file = "test_recv.jpeg"

    @classmethod
    def setup_class(cls):
        check_hdc_cmd(f"shell rm -rf {REMOTE_SNAPSHOT_PATH}")
        check_hdc_cmd(f"shell rm -rf {MEDIA_PHOTO_OTHER_DIR}")

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
            check_hdc_cmd(f"shell snapshot_display -f {REMOTE_SNAPSHOT_PATH}")

            output = get_shell_result(f"shell ls -l {REMOTE_SNAPSHOT_PATH}")
            assert "test.jpeg" in output

            check_hdc_cmd(f"shell mkdir -p {MEDIA_PHOTO_OTHER_DIR}")
            check_hdc_cmd(f"shell cp {REMOTE_SNAPSHOT_PATH} {MEDIA_PHOTO_OTHER_DIR}/test.jpeg")

            ls_output = get_shell_result(f"shell ls -l {MEDIA_PHOTO_OTHER_DIR}")
            assert "test.jpeg" in ls_output

            recv_cmd = f"file recv {MEDIA_PHOTO_OTHER_DIR}/test.jpeg {recv_file}"
            assert check_hdc_cmd(recv_cmd)

            assert os.path.exists(recv_file)

        finally:
            if os.path.exists(recv_file):
                os.remove(recv_file)
            # 清理远程文件
            check_hdc_cmd(f"shell rm -rf {REMOTE_SNAPSHOT_PATH}")
            check_hdc_cmd(f"shell rm -rf {MEDIA_PHOTO_OTHER_DIR}/test.jpeg")