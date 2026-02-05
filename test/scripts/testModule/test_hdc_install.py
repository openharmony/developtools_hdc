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
import sys
from utils import GP, check_app_install, check_app_install_multi, check_app_uninstall, \
    check_app_uninstall_multi, check_hdc_cmd, get_local_path, load_gp, check_app_not_exist, \
    check_app_dir_not_exist, get_bundle_info


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

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_install_hsp_not_exist(self):
        package_haps_dir = "not_exist.hsp"
        hap_name_default_default = "com.not.exist.hsp"
        assert check_app_not_exist(package_haps_dir, hap_name_default_default)

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_install_hap_not_exist(self):
        package_haps_dir = "not_exist.hap"
        hap_name_default_default = "com.not.exist.hap"
        assert check_app_not_exist(package_haps_dir, hap_name_default_default, "-s")

    @pytest.mark.L0
    @pytest.mark.repeat(2)
    def test_install_dir_not_exist(self):
        package_haps_dir = "abcdef_dir_not_exist"
        hap_name_default_default = "com.abcedf.dir.not.exist"
        assert check_app_dir_not_exist(package_haps_dir, hap_name_default_default)

    @pytest.mark.L0
    def test_install_options_format(self):
        package_hap = "AACommand07.hap"
        hap_module_name = "com.example.aacommand07"
        package_hsp = "libA_v10001.hsp"
        hsp_module_name = "com.example.liba"
        ### base cases ###
        # no option
        assert check_app_install(package_hap, hap_module_name)
        updateTime1 = int(get_bundle_info(hap_module_name, "updateTime"))
        # -r
        assert check_app_install(package_hap, hap_module_name, '-r')
        updateTime2 = (get_bundle_info(hap_module_name, "updateTime"))
        assert updateTime1 != 0 and updateTime2 != 0 and updateTime2 > updateTime1
        # -s
        assert check_app_install(package_hsp, hsp_module_name, '-s')
        # -cwd
        if sys.platform == 'win32':
            assert check_app_install(package_hap, hap_module_name, '-cwd .\\')
        else:
            assert check_app_install(package_hap, hap_module_name, '-cwd ./')
        # -w
        assert check_app_install(package_hap, hap_module_name, '"-w 180"')
        # -u
        assert check_app_install(package_hap, hap_module_name, '"-u 100"')
        assert get_bundle_info(hap_module_name, 'userInfo')[0]['bundleUserInfo']['userId'] == 100
        assert check_app_install(package_hap, hap_module_name, '"-u 102"')
        # install default active user when -u with nonexistent userId
        assert get_bundle_info(hap_module_name, 'userInfo')[0]['bundleUserInfo']['userId'] == 100

        # -p
        assert check_app_install(package_hap, hap_module_name, '-p')
        # -h: -h not install bundlename
        assert not check_app_install(package_hap, hap_module_name, '-h')
        ### multi options ###
        # -r -s
        assert check_app_install(package_hsp, hsp_module_name, "-r -s")
        # -p -r -w -u
        assert check_app_install(package_hap, hap_module_name, '-r "-w 180" "-u 100" -p')
        ### wrong format ###
        assert not check_app_install(package_hap, hap_module_name, '-p -r -w 180 -u 100')
        assert not check_app_install(package_hap, hap_module_name, '-p -r "-w 180" "-u 100"')

    @pytest.mark.L0
    def test_uninstall_options_format(self):
        package_hap = "AACommand07.hap"
        hap_module_name = "com.example.aacommand07"
        package_hsp = "libA_v10001.hsp"
        hsp_module_name = "com.example.liba"
        ### base cases ###
        # no option
        assert check_app_install(package_hap, hap_module_name)
        assert check_app_uninstall(hap_module_name)
        # -k
        assert check_app_install(package_hap, hap_module_name)
        assert check_app_uninstall(hap_module_name, '-k')
        # -s
        assert check_app_install(package_hsp, hsp_module_name, '-s')
        assert check_app_uninstall(hsp_module_name, '-s')
        # -n
        assert check_app_install(package_hap, hap_module_name)
        assert check_app_uninstall(hap_module_name, '-n')
        assert not check_app_uninstall(hap_module_name, '-n')
        # -v
        assert check_app_install(package_hsp, hsp_module_name, '-s')
        assert check_app_uninstall(hsp_module_name, '-s "-v 10001"')
        # -u
        assert check_app_install(package_hap, hap_module_name, '"-u 100"')
        assert check_app_uninstall(hap_module_name, '"-u 100"')
        assert check_app_install(package_hap, hap_module_name, '"-u 100"')
        assert check_app_uninstall(hap_module_name, '"-u 102"')
        # -h: -h not install bundlename
        assert check_app_install(package_hap, hap_module_name)
        assert not check_app_uninstall(hap_module_name, '-h')
        ### multi options ###
        # -k -s -u -v -n
        assert check_app_install(package_hsp, hsp_module_name, '-s')
        assert check_app_uninstall(hsp_module_name, '-k -s "-u 100" "-v 10001" -n')
        ### wrong format ###
        assert check_app_install(package_hsp, hsp_module_name, '-s')
        assert not check_app_uninstall(hsp_module_name, '-k -s -u 100 -v 10001 -n')
        assert not check_app_uninstall(hsp_module_name, '-n -k -s "-u 100" "-v 10001"')

    @pytest.mark.L0
    def test_install_chinese_dir(self):
        bundle_name = 'com.example.aacommand07'
        dir_name = '临时'
        file_name = '测试.hap'
        chinese_dir = get_local_path(dir_name)
        os.makedirs(chinese_dir, exist_ok = True)
        src = get_local_path('AACommand07.hap')
        dst = os.path.join(chinese_dir, file_name)
        shutil.copy(src, dst)
        assert check_app_install(dir_name, bundle_name)
        assert check_app_uninstall(bundle_name)
        shutil.rmtree(chinese_dir)
