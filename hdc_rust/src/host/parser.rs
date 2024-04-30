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
use super::translate;

use hdc::config::{self, HdcCommand};

use std::collections::HashMap;
use std::io::{self, Error, ErrorKind};
use std::str::FromStr;
use hdc::utils;

#[derive(Default, Debug, Clone)]
pub struct Parsed {
    pub options: Vec<String>,
    pub command: Option<HdcCommand>,
    pub parameters: Vec<String>,
}

lazy_static! {
    static ref CMD_MAP: HashMap<&'static str, HdcCommand> = {
        let mut map = HashMap::new();

        map.insert("version", HdcCommand::ClientVersion);
        map.insert("help", HdcCommand::KernelHelp);
        map.insert("discover", HdcCommand::KernelTargetDiscover);
        map.insert("start", HdcCommand::KernelServerStart);
        map.insert("kill", HdcCommand::KernelServerKill);
        map.insert("keygen", HdcCommand::ClientKeyGenerate);
        map.insert("list targets", HdcCommand::KernelTargetList);
        map.insert("checkserver", HdcCommand::KernelCheckServer);
        map.insert("checkdevice", HdcCommand::KernelCheckDevice);
        map.insert("wait", HdcCommand::KernelWaitFor);
        map.insert("tconn", HdcCommand::KernelTargetConnect);
        map.insert("any", HdcCommand::KernelTargetAny);
        map.insert("shell", HdcCommand::UnityExecute);
        map.insert("target boot", HdcCommand::UnityReboot);
        map.insert("target mount", HdcCommand::UnityRemount);
        map.insert("smode", HdcCommand::UnityRootrun);
        map.insert("tmode", HdcCommand::UnityRunmode);
        map.insert("bugreport", HdcCommand::UnityBugreportInit);
        map.insert("hilog", HdcCommand::UnityHilog);
        map.insert("file send", HdcCommand::FileInit);
        map.insert("file recv", HdcCommand::FileRecvInit);
        map.insert("fport", HdcCommand::ForwardInit);
        map.insert("rport", HdcCommand::ForwardRportInit);
        map.insert("rport ls", HdcCommand::ForwardRportList);
        map.insert("fport ls", HdcCommand::ForwardList);
        map.insert("fport rm", HdcCommand::ForwardRemove);
        map.insert("rport rm", HdcCommand::ForwardRportRemove);
        map.insert("install", HdcCommand::AppInit);
        map.insert("uninstall", HdcCommand::AppUninstall);
        map.insert("sideload", HdcCommand::AppSideload);
        map.insert("jpid", HdcCommand::JdwpList);
        map.insert("track-jpid", HdcCommand::JdwpTrack);
        map.insert("alive", HdcCommand::KernelEnableKeepalive);
        map.insert("update", HdcCommand::FlashdUpdateInit);
        map.insert("flash", HdcCommand::FlashdFlashInit);
        map.insert("erase", HdcCommand::FlashdErase);
        map.insert("format", HdcCommand::FlashdFormat);

        map
    };
}

const MAX_CMD_LEN: usize = 3;

// TODO: trial tree
pub fn split_opt_and_cmd(input: Vec<String>) -> Parsed {
    let mut cmd_opt: Option<HdcCommand> = None;
    let mut cmd_index = input.len();
    for len in 1..MAX_CMD_LEN {
        for st in 0..input.len() {
            if st + len > input.len() {
                break;
            }
            let cmd = input[st..st + len].join(" ");
            if let Some(command) = CMD_MAP.get(cmd.as_str()) {
                cmd_index = st;
                cmd_opt = Some(command.to_owned());
                if *command == HdcCommand::ForwardInit
                    || *command == HdcCommand::ForwardRportInit {
                    continue;
                } else {
                    break;
                }
            }
        }
        if cmd_opt.is_some()
            && cmd_opt.unwrap() != HdcCommand::ForwardInit
            && cmd_opt.unwrap() != HdcCommand::ForwardRportInit {
            break;
        }
    }
    Parsed {
        options: input[..cmd_index].to_vec(),
        command: cmd_opt,
        parameters: input[cmd_index..].to_vec(),
    }
}

pub fn parse_command(args: std::env::Args) -> io::Result<ParsedCommand> {
    let input = args.collect::<Vec<_>>()[1..].to_vec();
    let parsed = split_opt_and_cmd(input);
    match extract_global_params(parsed.options) {
        Ok(parsed_cmd) => {
            Ok(ParsedCommand {
                command: parsed.command,
                parameters: parsed.parameters,
                ..parsed_cmd
            })
        }
        Err(e) => Err(e),
    }
}

#[derive(Default, Debug, PartialEq)]
pub struct ParsedCommand {
    pub run_in_server: bool,
    pub launch_server: bool,
    pub spawned_server: bool,
    pub connect_key: String,
    pub log_level: usize,
    pub server_addr: String,
    pub command: Option<HdcCommand>,
    pub parameters: Vec<String>,
}

pub fn extract_global_params(opts: Vec<String>) -> io::Result<ParsedCommand> {
    let mut parsed_command = ParsedCommand {
        launch_server: true,
        log_level: 3,
        server_addr: format!("127.0.0.1:{}", config::SERVER_DEFAULT_PORT),
        ..Default::default()
    };
    let len = opts.len();
    for i in 0..len {
        let opt = opts[i].as_str();
        let arg = if opt.len() > 2 {
            &opt[2..]
        } else if i < len - 1 {
            opts[i + 1].as_str()
        } else {
            ""
        };
        if opt.starts_with("-h") {
            if arg == "verbose" {
                return Err(utils::error_other(translate::verbose()));
            } else {
                return Err(utils::error_other(translate::usage()));
            }
        } else if opt.starts_with("-v") {
            return Err(utils::error_other(config::get_version()));
        } else if opt.starts_with("-l") {
            if let Ok(level) = arg.parse::<usize>() {
                if level < config::LOG_LEVEL_ORDER.len() {
                    parsed_command.log_level = level;
                } else {
                    return Err(utils::error_other(format!(
                        "-l content loglevel incorrect\n\n{}",
                        translate::usage()
                    )));
                }
            } else {
                return Err(utils::error_other(format!(
                    "-l content loglevel incorrect\n\n{}",
                    translate::usage()
                )));
            }
        } else if opt.starts_with("-m") {
            parsed_command.run_in_server = true;
        } else if opt.starts_with("-p") {
            parsed_command.launch_server = false;
        } else if opt.starts_with("-t") {
            parsed_command.connect_key = arg.to_string();
        } else if opt.starts_with("-s") {
            match parse_server_listen_string(arg.to_string()) {
                Ok(server_addr) => parsed_command.server_addr = server_addr,
                Err(e) => {
                    return Err(utils::error_other(format!(
                        "{}\n\n{}",
                        e,
                        translate::usage()
                    )));
                }
            }
        } else if opt.starts_with("-b") {
            // server spawned by client, no stdout
            parsed_command.spawned_server = true;
        }
    }
    Ok(parsed_command)
}

fn check_port(port_str: String) -> io::Result<u16> {
    if let Ok(port) = port_str.parse::<u16>() {
        return Ok(port);
    }
    Err(Error::new(ErrorKind::Other, "-s content port incorrect"))
}

fn parse_server_listen_string(arg: String) -> io::Result<String> {
    let segments: Vec<&str> = arg.split(':').collect();
    let port_str = segments.last().unwrap().to_string();
    let port_len = port_str.len();
    let port = check_port(port_str)?;

    if segments.len() == 1 {
        return Ok(format!(
            // "{}{}:{}",
            // config::IPV4_MAPPING_PREFIX,
            "{}:{}",
            config::LOCAL_HOST,
            port
        ));
    }

    let ip_str = &arg[..arg.len() - port_len - 1];
    match std::net::IpAddr::from_str(ip_str) {
        Ok(ip_addr) => {
            if ip_addr.is_ipv4() || ip_addr.is_ipv6() {
                Ok(arg)
            } else {
                Err(Error::new(ErrorKind::Other, "-s content ip incorrect"))
            }
        }
        _ => Err(Error::new(ErrorKind::Other, "-s content ip incorrect")),
    }
}
