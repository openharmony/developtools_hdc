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
from utils import GP, check_shell_any_device, check_hdc_cmd, get_shell_result, load_gp, get_local_path, get_local_md5
from utils import server_loop
from utils import client_get_file
import threading
import os


class TestXportCommand:
    @pytest.mark.L0
    def test_fport_cmd_output(self):
        local_port = 18070
        remote_port = 11080
        fport_arg = f"tcp:{local_port} tcp:{remote_port}"
        assert check_hdc_cmd(f"fport {fport_arg}", "Forwardport result:OK")
        assert check_shell_any_device(f"netstat -an", "LISTENING", False)[0]
        assert check_shell_any_device(f"netstat -an", f"{local_port}", False)[0]
        assert check_hdc_cmd(f"fport ls", fport_arg)
        assert check_hdc_cmd(f"fport rm {fport_arg}", "success")

    @pytest.mark.L0
    def test_rport_cmd_output(self):
        local_port = 17090
        remote_port = 11080
        rport_arg = f"tcp:{local_port} tcp:{remote_port}"
        assert check_hdc_cmd(f"rport {rport_arg}", "Forwardport result:OK")
        netstat_line = get_shell_result(f'shell "netstat -anp | grep {local_port}"')
        assert "LISTEN" in netstat_line
        assert "hdcd" in netstat_line
        fport_list = get_shell_result(f"fport ls")
        assert "Reverse" in fport_list
        assert rport_arg in fport_list
        assert check_hdc_cmd(f"fport rm {rport_arg}", "success")

    @pytest.mark.L0
    def test_xport_data_comm(self):
        """
        test fport rport data communicate.

        1. fport host 17091(listen) -> daemon 11081;
        2. rport daemon 11081(listen) -> host 18000;
        3. pc client connect 17091, server listen 18000;
        4. Communication Link: client -> 17091 host -|- 11081 daemon 11081 -|- host -> 18000 server;
        """
        client_connect_port = 17091
        daemon_transmit_port = 11081
        server_listen_port = 18000

        # creat transmit env
        # fport
        fport_arg = f"tcp:{client_connect_port} tcp:{daemon_transmit_port}"
        assert check_hdc_cmd(f"fport {fport_arg}", "Forwardport result:OK")
        assert check_shell_any_device(f"netstat -an", "LISTENING", False)[0]
        assert check_shell_any_device(f"netstat -an", f"{client_connect_port}", False)[0]
        assert check_hdc_cmd(f"fport ls", fport_arg)

        # rport
        rport_arg = f"tcp:{daemon_transmit_port} tcp:{server_listen_port}"
        assert check_hdc_cmd(f"rport {rport_arg}", "Forwardport result:OK")
        netstat_line = get_shell_result(f'shell "netstat -anp | grep {daemon_transmit_port}"')
        assert "LISTEN" in netstat_line
        assert "hdcd" in netstat_line
        fport_list = get_shell_result(f"fport ls")
        assert "Reverse" in fport_list
        assert rport_arg in fport_list

        # transmit file start
        file_name = "medium"
        file_save_name = f"{file_name}_recv_fport"
        file_path = get_local_path(file_save_name)
        if os.path.exists(file_path):
            print(f"client: file {file_path} exist, delete")
            try:
                os.remove(file_path)
            except OSError as error:
                print(f"delete {file_path} failed: {error.strerror}")
                assert check_hdc_cmd(f"fport rm {fport_arg}", "success")
                assert check_hdc_cmd(f"fport rm {rport_arg}", "success")
                assert False

        server_thread = threading.Thread(target=server_loop, args=(server_listen_port,))
        server_thread.start()

        client_get_file(client_connect_port, file_name, file_save_name)
        server_thread.join()

        assert check_hdc_cmd(f"fport rm {fport_arg}", "success")
        assert check_hdc_cmd(f"fport rm {rport_arg}", "success")

        ori_file_md5 = get_local_md5(get_local_path(file_name))
        new_file = get_local_path(file_save_name)
        assert os.path.exists(new_file)
        new_file_md5 = get_local_md5(new_file)
        print(f"ori_file_md5:{ori_file_md5}, new_file_md5:{new_file_md5}")
        assert ori_file_md5 == new_file_md5

    @pytest.mark.L0
    def test_fport_cmd(self):
        fport_list = []
        rport_list = []
        start_port = 10000
        end_port = 10020
        for i in range(start_port, end_port):
            fport = f"tcp:{i+100} tcp:{i+200}"
            rport = f"tcp:{i+300} tcp:{i+400}"
            localabs = f"tcp:{i+500} localabstract:{f'helloworld.com.app.{i+600}'}"
            fport_list.append(fport)
            rport_list.append(rport)
            fport_list.append(localabs)

        for fport in fport_list:
            assert check_hdc_cmd(f"fport {fport}", "Forwardport result:OK")
            assert check_hdc_cmd(f"fport {fport}", "TCP Port listen failed at")
            assert check_hdc_cmd("fport ls", fport)

        for fport in fport_list:
            assert check_hdc_cmd(f"fport rm {fport}", "success")
            assert not check_hdc_cmd("fport ls", fport)

        for rport in rport_list:
            assert check_hdc_cmd(f"rport {rport}", "Forwardport result:OK")
            assert check_hdc_cmd(f"rport {rport}", "TCP Port listen failed at")
            assert check_hdc_cmd("rport ls", rport) or check_hdc_cmd("fport ls", rport)

        for rport in rport_list:
            assert check_hdc_cmd(f"fport rm {rport}", "success")
            assert not check_hdc_cmd("rport ls", fport) and not check_hdc_cmd("fport ls", fport)

        task_str1 = "tcp:33333 tcp:33333"
        assert check_hdc_cmd(f"fport {task_str1}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport rm {task_str1}", "success")
        assert check_hdc_cmd(f"fport {task_str1}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport rm {task_str1}", "success")

        task_str2 = "tcp:44444 tcp:44444"
        assert check_hdc_cmd(f"rport {task_str2}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport rm {task_str2}", "success")
        assert check_hdc_cmd(f"rport {task_str2}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport rm {task_str2}", "success")
