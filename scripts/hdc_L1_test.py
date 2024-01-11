from dev_hdc_test import *
import argparse
class TestCommands:
    r"""Usage:
    pip install pytest pytest-testreport
    python hdc_L1_test.py
    """
    def test_list_targets(self):
        assert check_hdc_targets()

    @pytest.mark.repeat(5)
    def test_empty_file(self):
        assert check_hdc_cmd(f"file send {get_local_path('empty')} {get_remote_path('it_empty')}")
        assert check_hdc_cmd(f"file recv {get_remote_path('it_empty')} {get_local_path('empty_recv')}")

    @pytest.mark.repeat(5)
    def test_small_file(self):
        assert check_hdc_cmd(f"file send {get_local_path('small')} {get_remote_path('it_small')}")
        assert check_hdc_cmd(f"file recv {get_remote_path('it_small')} {get_local_path('small_recv')}")

    @pytest.mark.repeat(1)
    def test_large_file(self):
        assert check_hdc_cmd(f"file send {get_local_path('large')} {get_remote_path('it_large')}")
        assert check_hdc_cmd(f"file recv {get_remote_path('it_large')} {get_local_path('large_recv')}")     

    @pytest.mark.repeat(5)
    def test_hap_install(self):
        assert check_hdc_cmd(f"install -r {get_local_path('entry-default-signed-debug.hap')}",
                             bundle="com.hmos.diagnosis")
    @pytest.mark.repeat(5)
    def test_app_cmd(self):
        assert check_app_install("entry-default-signed-debug.hap", "com.hmos.diagnosis")
        assert check_app_uninstall("com.hmos.diagnosis")

        assert check_app_install("entry-default-signed-debug.hap", "com.hmos.diagnosis", "-r")
        assert check_app_uninstall("com.hmos.diagnosis")

        assert check_app_install("analyticshsp-default-signed.hsp", "com.huawei.hms.hsp.analyticshsp", "-s")
        assert check_app_uninstall("com.huawei.hms.hsp.analyticshsp", "-s")

    def test_server_kill(self):
        assert check_hdc_cmd("kill", "Kill server finish")
        os.system("hdc start server")

    def test_target_cmd(self):
        check_hdc_cmd("target boot")
        time.sleep(20)
        assert (check_hdc_cmd("target mount", "Mount finish") or
                check_hdc_cmd("target mount", "[Fail]Operate need running as root"))

    def test_version_cmd(self):
        assert check_hdc_cmd("-v", "Ver: 2.0.0a")
        assert check_hdc_cmd("version", "Ver: 2.0.0a")
        assert check_hdc_cmd("checkserver", "Ver: 2.0.0a")

    def test_fport_cmd(self):
        fport_list = []
        start_port = 10000
        end_port = 10020
        for i in range(start_port, end_port):
            fport = f"tcp:{i+100} tcp:{i+200}"
            rport = f"tcp:{i+300} tcp:{i+400}"
            localabs = f"tcp:{i+500} localabstract:{f'helloworld.com.app.{i+600}'}"
            fport_list.append(fport)
            fport_list.append(rport)
            fport_list.append(localabs)
        
        for fport in fport_list:
            assert check_hdc_cmd(f"fport {fport}", "Forwardport result:OK")
            assert check_hdc_cmd("fport ls", fport)

        for fport in fport_list:
            assert check_hdc_cmd(f"fport rm {fport}", "success")
            assert not check_hdc_cmd("fport ls", fport)
        
        single_port = 10000
        fport = f"tcp:{single_port} tcp:{single_port+100}"
        not_full_port = f"tcp:{single_port}"
        over_number_port = f"tcp:{single_port+60000} tcp:{single_port+100}"
        assert check_hdc_cmd(f"fport {not_full_port}", "[Fail]Forward parament failed")
        assert check_hdc_cmd(f"fport {fport}", "Forwardport result:OK")
        assert check_hdc_cmd(f"fport {fport}", f"[Fail]TCP Port listen failed at {single_port}")
        assert check_hdc_cmd("fport ls", fport)
        assert check_hdc_cmd(f"fport rm {fport}", "success")
    

    def setup_class(self):
        print("setting up env ...")
        check_hdc_cmd("shell rm -rf /data/local/tmp/it_*")
        GP.load()

    def teardown_class(self):
        pass

def main():
    if check_library_installation("pytest"):
        exit(1)
    if check_library_installation("pytest-testreport"):
        exit(1)
    if not os.path.exists(GP.local_path):
        prepare_source()
    choice_default = ""
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=1,
                        help='test times')
    parser.add_argument('--verbose', '-v', default='hdc_L1_test.py',
                        help='filename')
    parser.add_argument('--desc', '-d', default='Test for function.',
                        help='Add description on report')    
    args = parser.parse_args()
    
    for i in range(args.count):
        print(f"------------The {i}/{args.count} Test-------------")
        timestamp = time.time()
        pytest_args = ["--verbose", args.verbose,
             '--report=report.html',
             '--title=test_report',
             '--tester=tester001',
             f'--desc={args.verbose}:{args.desc}',
             '--template=1']
        pytest.main(pytest_args)
    input("test over, press Enter key to continue")

if __name__ == "__main__":
    main()