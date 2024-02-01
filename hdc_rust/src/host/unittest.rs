/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#[cfg(test)]
mod parser_tests {
    use crate::{
        host_app::HostAppTask,
        parser::{self, Parsed, ParsedCommand},
    };
    use hdc::config::{self, HdcCommand};

    #[test]
    fn if_parse_cmd_param_works() {
        let input = "file recv file1 /data/local/tmp/file"
            .split(" ")
            .map(|s| s.to_string())
            .collect();
        let expected = Parsed {
            options: vec![],
            command: Some(HdcCommand::FileInit),
            parameters: "file recv file1 /data/local/tmp/file"
                .split(" ")
                .map(|s| s.to_string())
                .collect(),
        };
        let actual = parser::split_opt_and_cmd(input);
        assert_eq!(actual.options, expected.options);
        assert_eq!(actual.command, expected.command);
        assert_eq!(actual.parameters, expected.parameters);
    }

    #[test]
    fn if_parse_opt_cmd_works() {
        let input = "-l5 checkserver"
            .split(" ")
            .map(|s| s.to_string())
            .collect();
        let expected = Parsed {
            options: vec!["-l5".to_string()],
            command: Some(HdcCommand::KernelCheckServer),
            parameters: vec![],
        };
        let actual = parser::split_opt_and_cmd(input);
        assert_eq!(actual.options, expected.options);
        assert_eq!(actual.command, expected.command);
    }

    #[test]
    fn if_parse_opt_cmd_param_works() {
        let input = "-l5 file recv file1 /data/local/tmp/file"
            .split(" ")
            .map(|s| s.to_string())
            .collect();
        let expected = Parsed {
            options: vec!["-l5".to_string()],
            command: Some(HdcCommand::FileInit),
            parameters: "file recv file1 /data/local/tmp/file"
                .split(" ")
                .map(|s| s.to_string())
                .collect(),
        };
        let actual = parser::split_opt_and_cmd(input);
        assert_eq!(actual.options, expected.options);
        assert_eq!(actual.command, expected.command);
        assert_eq!(actual.parameters, expected.parameters);
    }

    #[test]
    fn if_extract_opt_lt_works() {
        let opts = "-l5 -t 123456".split(" ").map(|s| s.to_string()).collect();
        let expected = ParsedCommand {
            run_in_server: false,
            launch_server: true,
            connect_key: "123456".to_string(),
            log_level: 5,
            server_addr: format!("0.0.0.0:{}", config::SERVER_DEFAULT_PORT),
            ..Default::default()
        };
        let actual = parser::extract_global_params(opts).unwrap();
        assert_eq!(actual, expected);
    }

    #[test]
    fn if_extract_opt_sm_works() {
        let opts = "-s 127.0.0.1:23333 -m"
            .split(" ")
            .map(|s| s.to_string())
            .collect();
        let expected = ParsedCommand {
            run_in_server: true,
            launch_server: true,
            connect_key: "".to_string(),
            log_level: 3,
            server_addr: "127.0.0.1:23333".to_string(),
            ..Default::default()
        };
        let actual = parser::extract_global_params(opts).unwrap();
        assert_eq!(actual, expected);
    }

    #[test]
    fn if_extract_opt_port_works() {
        let opts = "-s 23333".split(" ").map(|s| s.to_string()).collect();
        let expected = ParsedCommand {
            run_in_server: false,
            launch_server: true,
            connect_key: "".to_string(),
            log_level: 3,
            server_addr: "127.0.0.1:23333".to_string(),
            ..Default::default()
        };
        let actual = parser::extract_global_params(opts).unwrap();
        assert_eq!(actual, expected);
    }

    #[test]
    fn if_extract_opt_ipv6_works() {
        let opts = "-s FC00:0:130F:0:0:9C0:876A:130B:23333 -p"
            .split(" ")
            .map(|s| s.to_string())
            .collect();
        let expected = ParsedCommand {
            run_in_server: false,
            launch_server: false,
            connect_key: "".to_string(),
            log_level: 3,
            server_addr: "FC00:0:130F:0:0:9C0:876A:130B:23333".to_string(),
            ..Default::default()
        };
        let actual = parser::extract_global_params(opts).unwrap();
        assert_eq!(actual, expected);
    }

    #[test]
    fn if_extract_opt_invalid_ipv6_works() {
        let opts = "-s FC00:0:130F:0:0:9C0:876A:23333"
            .split(" ")
            .map(|s| s.to_string())
            .collect();
        let actual = parser::extract_global_params(opts);
        assert!(actual.is_err());
    }

    #[test]
    fn if_extract_opt_invalid_port_works() {
        let opts = "-s 233333".split(" ").map(|s| s.to_string()).collect();
        let actual = parser::extract_global_params(opts);
        assert!(actual.is_err());
    }

    #[test]
    fn if_init_install_works() {
        let mut task = HostAppTask::new(0, 0);
        task.init_install(&String::from("-cwd \"/home/\" 1234.hap"));

        assert_eq!(task.transfer.local_path, "/home/1234.hap");
        let ret = task
            .transfer
            .transfer_config
            .optional_name
            .ends_with(".hap");
        assert_eq!(ret, true);
    }
}
