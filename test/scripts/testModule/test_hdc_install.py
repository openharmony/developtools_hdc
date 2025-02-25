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
import pytest
from utils import GP, check_app_install, check_app_install_multi, check_app_uninstall, \
    check_app_uninstall_multi, check_hdc_cmd, get_local_path, load_gp


class TestInstallBase:
    hap_tables = {
        "AACommand07.hap" : "com.example.aacommand07",
        "AACommand08.hap" : "com.example.aacommand08"
    }
    hsp_tables = {
        "libA_v10001.hsp" : "com.example.liba",
        "libB_v10001.hsp" : "com.example.libb",
    }
    hsp_hap_tables = {
        "AACommandpackage.hap" : "com.example.actsaacommandtestatest",
        "libB_v10001.hsp" : "com.example.libb",
    }

    # 来自原hdc_normal_test.py的基础install 用例
    @pytest.mark.L0
    @pytest.mark.repeat(2)
    @pytest.mark.parametrize("package_hap, hap_name_default", hap_tables.items())
    def test_hap_install(self, package_hap, hap_name_default):
        assert check_hdc_cmd(f"install -r {get_local_path(f'{package_hap}')}",
                                bundle=f"{hap_name_default}")
        assert check_app_uninstall(f"{hap_name_default}")

    @pytest.mark.L1
    @pytest.mark.repeat(2)
    @pytest.mark.parametrize("package_hap, hap_name_default", hap_tables.items())
    def test_install_hap(self, package_hap, hap_name_default):

        # default
        assert check_app_install(package_hap, hap_name_default)
        assert check_app_uninstall(hap_name_default)

        # -r
        assert check_app_install(package_hap, hap_name_default, "-r")
        assert check_app_uninstall(hap_name_default)

        # -k
        assert check_app_install(package_hap, hap_name_default, "-r")
        assert check_app_uninstall(hap_name_default, "-k")

        # -s
        assert check_app_install(package_hap, hap_name_default, "-s")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    @pytest.mark.parametrize("package_hsp, hsp_name_default", hsp_tables.items())
    def test_install_hsp(self, package_hsp, hsp_name_default):
        assert check_app_install(package_hsp, hsp_name_default, "-s")
        assert check_app_uninstall(hsp_name_default, "-s")
        assert check_app_install(package_hsp, hsp_name_default)

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_install_multi_hap(self):
        # default multi hap
        assert check_app_install_multi(self.hap_tables)
        assert check_app_uninstall_multi(self.hap_tables)
        assert check_app_install_multi(self.hap_tables, "-s")
        assert check_app_install_multi(self.hap_tables, "-r")
        assert check_app_uninstall_multi(self.hap_tables, "-k")


    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_install_multi_hsp(self):
        # default multi hsp -s
        assert check_app_install_multi(self.hsp_tables, "-s")
        assert check_app_uninstall_multi(self.hsp_tables, "-s")
        assert check_app_install_multi(self.hsp_tables)

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_install_hsp_and_hap(self):
        #default multi hsp and hsp
        assert check_app_install_multi(self.hsp_hap_tables)
        assert check_app_install_multi(self.hsp_hap_tables, "-s")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_install_dir(self):
        package_haps_dir = "app_dir"
        hap_name_default_default = "com.example.aacommand07"
        assert check_app_install(package_haps_dir, hap_name_default_default)
        assert check_app_uninstall(hap_name_default_default)