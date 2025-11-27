/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#include "translate.h"
#include "host_updater.h"

namespace Hdc {
namespace TranslateCommand {
string Usage()
{
    string ret = "";
    ret = "\n                         OpenHarmony device connector(hdc) ...\n\n"
            "---------------------------------global commands:----------------------------------\n"
            " -h/help [verbose]                     - Print hdc help, 'verbose' for more other cmds\n"
            " -v/version                            - Print hdc version\n"
            " -l[0-6]                               - Set runtime log level\n"
            " -t connectkey                         - Use device with given connect key\n"
            " checkserver                           - Check client-server version\n"
            "\n"
            "---------------------------------component commands:-------------------------------\n"
            "session commands(on server):\n"
            " list targets [-v]                     - List all devices status, -v for detail\n"
            " tconn key [-remove]                   - Connect device via key, TCP use ip:port\n"
            "                                         example:192.168.0.100:10178/192.168.0.100\n"
            "                                         USB connect automatic, TCP need to connect manually\n"
            "                                         If with '-remove', will remove connection"
#ifdef HDC_SUPPORT_UART
            "\n"
            "                                         UART connect need connect manually\n"
            "                                         Baud Rate can be specified with commas\n"
            "                                         key format: <Port Name>[,Baud Rate]\n"
            "                                         example: tconn COM5,921600\n"
            "                                         Default Baud Rate is 921600\n"
            "\n"
#endif
            " start [-r]                            - Start server. If with '-r', will be restart server\n"
            " kill [-r]                             - Kill server. If with '-r', will be restart server\n"
#ifdef __OHOS__
            " -s [ip:]port | uds                    - Set hdc server listen config\n"
#else
            " -s [ip:]port                          - Set hdc server listen config\n"
#endif
            "\n"
            "service commands(on daemon):\n"
            " target mount                          - Set /system /vendor partition read-write\n"
            " wait                                  - Wait for the device to become available\n"
            " target boot [-bootloader|-recovery]   - Reboot the device or boot into bootloader\\recovery.\n"
            " target boot [MODE]                    - Reboot the into MODE.\n"
            " smode [-r]                            - Restart daemon with root permissions, '-r' to cancel root\n"
            "                                         permissions\n"
            " tmode usb                             - Reboot the device, listening on USB\n"
            " tmode port [port]                     - Reboot the device, listening on TCP port\n"
            " tmode port close                      - Close TCP port\n"
            "\n"
            "---------------------------------task commands:-------------------------------------\n"
            "file commands:\n"
            " file send [option] local remote       - Send file to device\n"
            " file recv [option] remote local       - Recv file from device\n"
            "                                         option is -a|-s|-z\n"
            "                                         -a: hold target file timestamp\n"
            "                                         -sync: just update newer file\n"
            "                                         -z: compress transfer, not improve transmission efficiency\n"
            "                                         for compressed files\n"
            "                                         -m: mode sync\n"
            "                                         -cwd: specify the working directory\n"
            "                                         -b: send/receive file to debug application directory\n"
            "\n"
            "forward commands:\n"
            " fport localnode remotenode            - Forward local traffic to remote device\n"
            " rport remotenode localnode            - Reserve remote traffic to local host\n"
            "                                         node config name format 'schema:content'\n"
            "                                         examples are below:\n"
            "                                         tcp:<port>\n"
            "                                         localfilesystem:<unix domain socket name>\n"
            "                                         localreserved:<unix domain socket name>\n"
            "                                         localabstract:<unix domain socket name>\n"
            "                                         dev:<device name>\n"
            "                                         jdwp:<pid> (remote only)\n"
            "                                         ark:pid@tid@Debugger\n"
            " fport ls                              - Display forward/reverse tasks\n"
            " fport rm taskstr                      - Remove forward/reverse task by taskstring\n"
            "\n"
            "app commands:\n"
            " install [option] src                  - Send package(s) to device and install them\n"
            "                                         src examples: single or multiple packages and directories\n"
            "                                         (.hap .hsp .app)\n"
            "                                         option is -r|-s|-w|-u|-p|-h|-cwd\n"
            "                                         -r: replace existing application\n"
            "                                         -s: install shared bundle for multi-apps\n"
            "                                         -w: specify waitting time for installation\n"
            "                                         -u: specify a user id\n"
            "                                         -p: specify a bundle path or direction\n"
            "                                         -h: list available options of 'bm install' command\n"
            "                                         -cwd: specify the working directory\n"
            " uninstall [option] package            - Remove application package from device\n"
            "                                         option is -n|-m|-k|-s|-v|-u|-h\n"
            "                                         -n: uninstall a bundle by bundle name\n"
            "                                         -m: uninstall a module by module name\n"
            "                                         -k: keep the data and cache directories\n"
            "                                         -s: remove shared bundle\n"
            "                                         -v: uninstall a inter-application shared library by versionCode\n"
            "                                         -u: specify a user id\n"
            "                                         -h: list available options of 'bm uninstall' command\n"
            "\n"
            "debug commands:\n"
#ifdef _WIN32
            " hilog [-h|parse]                      - Show device log, -h for detail\n"
            "                                         parse: parse local hilog files\n"
#else
            " hilog [-h]                            - Show device log, -h for detail\n"
#endif
            " shell [-b bundlename] [COMMAND...]    - Run shell command (interactive shell if no command given)\n"
            "                                         -b: run command in specified debug application bundle path\n"
            "                                             bundle parameter only support non-interactive shell\n"
            " bugreport [FILE]                      - Return all information from the device, stored in file if "
            "FILE is specified\n"
            " jpid                                  - List PIDs of processes hosting a JDWP transport\n"
            " track-jpid [-a|-p]                    - Track PIDs of debug processes hosting a JDWP transport\n"
            "                                         -a: include debug and release processes\n"
            "                                         -p: don't display debug and release tags\n"
#ifdef __OHOS__
            "\n";
#else
            "\n"
            "security commands:\n"
            " keygen FILE                           - Generate public/private key; key stored in FILE and FILE.pub\n"
            "\n";
#endif
    return ret;
}

string Verbose()
{
    string ret = "\n                         OpenHarmony device connector(hdc) ...\n\n"
        "---------------------------------global commands:----------------------------------\n"
        " -h/help [verbose]                     - Print hdc help, 'verbose' for more other cmds\n"
        " -v/version                            - Print hdc version\n"
        " -l[0-6]                               - Set runtime log level\n"
        " -t connectkey                         - Use device with given connect key\n"
        " checkserver                           - Check client-server version\n"
        " checkdevice                           - Check server-daemon version(only uart)\n"
        " any                                   - Try to connect the first ready device(only uart)\n"
        "\n"
        "---------------------------------component commands:-------------------------------\n"
        "session commands(on server):\n"
        " discover                              - Discover devices listening on TCP via LAN broadcast\n"
        " list targets [-v]                     - List all devices status, -v for detail\n"
        " tconn key [-remove]                   - Connect device via key, TCP use ip:port\n"
        "                                         example:192.168.0.100:10178/192.168.0.100\n"
        "                                         USB connect automatic, TCP need to connect manually\n"
        "                                         If with '-remove', will remove connection"
#ifdef HDC_SUPPORT_UART
        "\n"
        "                                         UART connect need connect manually\n"
        "                                         Baud Rate can be specified with commas\n"
        "                                         key format: <Port Name>[,Baud Rate]\n"
        "                                         example: tconn COM5,921600\n"
        "                                         Default Baud Rate is 921600\n"
        "\n"
#endif
        " start [-r]                            - Start server. If with '-r', will be restart server\n"
        " kill [-r]                             - Kill server. If with '-r', will be restart server\n"
#ifdef __OHOS__
        " -s [ip:]port | uds                    - Set hdc server listen config\n"
#else
        " -s [ip:]port                          - Set hdc server listen config\n"
#endif
        " -e ip                                 - Set the IP address for which the host listens during TCP\n"
        "                                         port forwarding. The default value is 127.0.0.1,\n"
        "                                         cannot run in single client mode\n"
        " -m                                    - Start server at foreground\n"
        " -p                                    - Skip the server startup, run in single client mode\n"
        "\n"
        "service commands(on daemon):\n"
        " wait                                  - Wait for the device to become available\n"
        " target mount                          - Set /system /vendor partition read-write\n"
        " target boot [-bootloader|-recovery]   - Reboot the device or boot into bootloader\\recovery\n"
        " target boot [MODE]                    - Reboot the into MODE\n"
        " smode [-r]                            - Restart daemon with root permissions, '-r' to cancel root\n"
        "                                         permissions\n"
        " tmode usb                             - Reboot the device, listening on USB\n"
        " tmode port [port]                     - Reboot the device, listening on TCP port\n"
        " tmode port close                      - Close TCP port\n"
        "\n"
        "---------------------------------task commands:-------------------------------------\n"
        "file commands:\n"
        " file send [option] local remote       - Send file to device\n"
        " file recv [option] remote local       - Recv file from device\n"
        "                                         option is -a|-s|-z\n"
        "                                         -a: hold target file timestamp\n"
        "                                         -sync: just update newer file\n"
        "                                         -z: compress transfer, not improve transmission efficiency\n"
        "                                         for compressed files\n"
        "                                         -m: mode sync\n"
        "                                         -cwd: specify the working directory\n"
        "                                         -b: send/receive file to debug application directory\n"
        "\n"
        "forward commands:\n"
        " fport localnode remotenode            - Forward local traffic to remote device\n"
        " rport remotenode localnode            - Reserve remote traffic to local host\n"
        "                                         node config name format 'schema:content'\n"
        "                                         examples are below:\n"
        "                                         tcp:<port>\n"
        "                                         localfilesystem:<unix domain socket name>\n"
        "                                         localreserved:<unix domain socket name>\n"
        "                                         localabstract:<unix domain socket name>\n"
        "                                         dev:<device name>\n"
        "                                         jdwp:<pid> (remote only)\n"
        "                                         ark:pid@tid@Debugger\n"
        " fport ls                              - Display forward/reverse tasks\n"
        " fport rm taskstr                      - Remove forward/reverse task by taskstring\n"
        "\n"
        "app commands:\n"
        " install [option] src                  - Send package(s) to device and install them\n"
        "                                         src examples: single or multiple packages and directories\n"
        "                                         (.hap .hsp .app)\n"
        "                                         option is -r|-s|-w|-u|-p|-h|-cwd\n"
        "                                         -r: replace existing application\n"
        "                                         -s: install shared bundle for multi-apps\n"
        "                                         -w: specify waitting time for installation\n"
        "                                         -u: specify a user id\n"
        "                                         -p: specify a bundle path or direction\n"
        "                                         -h: list available options of 'bm install' command\n"
        "                                         -cwd: specify the working directory\n"
        " uninstall [option] package            - Remove application package from device\n"
        "                                         option is -n|-m|-k|-s|-v|-u|-h\n"
        "                                         -n: uninstall a bundle by bundle name\n"
        "                                         -m: uninstall a module by module name\n"
        "                                         -k: keep the data and cache directories\n"
        "                                         -s: remove shared bundle\n"
        "                                         -v: uninstall a inter-application shared library by versionCode\n"
        "                                         -u: specify a user id\n"
        "                                         -h: list available options of 'bm uninstall' command\n"
        "\n"
        "debug commands:\n"
#ifdef _WIN32
        " hilog [-h|parse]                      - Show device log, -h for detail\n"
        "                                         parse: parse local hilog files\n"
#else
        " hilog [-h]                            - Show device log, -h for detail\n"
#endif
        " shell [-b bundlename] [COMMAND...]    - Run shell command (interactive shell if no command given)\n"
        "                                         -b: run command in specified debug application bundle path\n"
        "                                             bundle parameter only support non-interactive shell\n"
        " bugreport [FILE]                      - Return all information from the device, stored in file if FILE "
        "is specified\n"
        " jpid                                  - List PIDs of processes hosting a JDWP transport\n"
        " track-jpid [-a|-p]                    - Track PIDs of debug processes hosting a JDWP transport\n"
        "                                         -a: include debug and release processes\n"
        "                                         -p: don't display debug and release tags\n"
        " sideload [PATH]                       - Sideload the given full OTA package\n"
        "\n"
#ifndef __OHOS__
        "security commands:\n"
        " keygen FILE                           - Generate public/private key; key stored in FILE and FILE.pub\n"
        "\n"
#endif
        "---------------------------------flash commands:------------------------------------\n"
        "flash commands:\n"
        " update packagename                    - Update system by package\n"
        " flash [-f] partition imagename        - Flash partition by image\n"
        " erase [-f] partition                  - Erase partition\n"
        " format [-f] partition                 - Format partition\n"
        "---------------------------------external commands:------------------------------------\n"
        "extconn key                            - Connect external device via key, TCP use ip:port(remian)\n"
        "-S [ip:]port                           - Set hdc external server listen config\n"
        "\n"
        "---------------------------------environment variables:-----------------------------\n"
#ifdef _WIN32
        "OHOS_HDC_SERVER_PORT                   - Set hdc server listen port(range 1~65535, default 8710)\n"
        "OHOS_HDC_LOG_LEVEL                     - Set hdc server log level(range 0~5, default 5)\n"
        "OHOS_HDC_HEARTBEAT                     - Set whether the hdc heartbeat function is disabled\n"
        "                                         (1: disabled, other: enabled, default: enabled)\n"
        "OHOS_HDC_CMD_RECORD                    - Set whether the hdc command record function is enabled\n"
        "                                         (1: enabled, other: disabled, default: disabled)\n"
#ifdef HDC_SUPPORT_ENCRYPT_TCP
        "OHOS_HDC_ENCRYPT_CHANNEL               - Set whether to use an encrypted channel "
        "when using a TCP connection\n"
        "                                         (1: enabled, other: disabled, default: disabled)\n"
#endif
#ifdef FEATURE_HOST_LOG_COMPRESS
        "OHOS_HDC_LOG_LIMIT                     - Set the number of logs that trigger the total"
        " log file size statistics (default 300 (files))\n"
#endif
#else
        "$OHOS_HDC_SERVER_PORT                  - Set hdc server listen port(range 1~65535, default 8710)\n"
        "$OHOS_HDC_LOG_LEVEL                    - Set hdc server log level(range 0~5, default 5)\n"
        "$OHOS_HDC_HEARTBEAT                    - Set whether the hdc heartbeat function is disabled\n"
        "                                         (1: disabled, other: enabled, default: enabled)\n"
        "$OHOS_HDC_CMD_RECORD                   - Set whether the hdc command record function is enabled\n"
        "                                         (1: enabled, other: disabled, default: disabled)\n"
#ifdef HDC_SUPPORT_ENCRYPT_TCP
        "$OHOS_HDC_ENCRYPT_CHANNEL              - Set whether to use an encrypted channel "
        "when using a TCP connection\n"
        "                                         (1: enabled, other: disabled, default: disabled)\n"
#endif
#ifdef FEATURE_HOST_LOG_COMPRESS
        "$OHOS_HDC_LOG_LIMIT                    - Set the number of logs that trigger the total"
        " log file size statistics (default 300 (files))\n"
#endif
#endif
        "\n";
    return ret;
}

string TargetConnect(FormatCommand *outCmd)
{
    string stringError;
    if (Base::StringEndsWith(outCmd->parameters, " -remove")) {
        outCmd->parameters = outCmd->parameters.substr(0, outCmd->parameters.size() - CMD_REMOTE_SIZE);
        outCmd->cmdFlag = CMD_KERNEL_TARGET_DISCONNECT;
    } else {
        outCmd->cmdFlag = CMD_KERNEL_TARGET_CONNECT;
        constexpr int maxKeyLength = 50; // 50: tcp max=21,USB max=8bytes, serial device name maybe long
        if (outCmd->parameters.size() > maxKeyLength) {
            stringError = "Error connect key's size";
            outCmd->bJumpDo = true;
        }
    }
    size_t pos = outCmd->parameters.find(":");
    if (pos != std::string::npos) {
        // tcp mode
        string ip = outCmd->parameters.substr(0, pos);
        if (ip == "localhost") {
            ip = "127.0.0.1";
            outCmd->parameters.replace(0, pos, ip);
        }
        if (!Base::IsValidIpv4(ip)) {
            stringError = "[E001104]:IP address incorrect";
            outCmd->bJumpDo = true;
            return stringError;
        }
        string sport = outCmd->parameters.substr(pos + 1);
        if (Base::GetCaller() == Base::Caller::SERVER) {
            WRITE_LOG(LOG_INFO, "TargetConnect ip:%s port:%s", Hdc::MaskString(ip).c_str(), sport.c_str());
        } else {
            WRITE_LOG(LOG_INFO, "TargetConnect ip:%s port:%s", ip.c_str(), sport.c_str());
        }
        
        if (sport.empty() || !Base::IsDigitString(sport)) {
            stringError = "Port incorrect";
            outCmd->bJumpDo = true;
            return stringError;
        }
        int port = static_cast<int>(strtol(sport.c_str(), nullptr, 10));
        sockaddr_in addr;
        if ((port <= 0 || port > MAX_IP_PORT) || uv_ip4_addr(ip.c_str(), port, &addr) < 0) {
            stringError = "IP:Port incorrect";
            outCmd->bJumpDo = true;
        }
    }
    return stringError;
}

string ForwardPort(const char *input, FormatCommand *outCmd)
{
    string stringError;
    const char *pExtra = input + 6;  // CMDSTR_FORWARD_FPORT CMDSTR_FORWARD_RPORT + " " size
    if (!strncmp(input, CMDSTR_FORWARD_FPORT.c_str(), CMDSTR_FORWARD_FPORT.size()) && !strcmp(pExtra, "ls")) {
        outCmd->cmdFlag = CMD_FORWARD_LIST;
    } else if (!strncmp(input, CMDSTR_FORWARD_FPORT.c_str(), CMDSTR_FORWARD_FPORT.size()) &&
        !strncmp(pExtra, "rm", 2)) { // 2: "rm" size
        outCmd->cmdFlag = CMD_FORWARD_REMOVE;
        if (strcmp(pExtra, "rm")) {
            outCmd->parameters = input + FORWORD_PORT_RM_BUF_SIZE;
        }
    } else {
        const char *p = input + FORWORD_PORT_OTHER_BUF_SIZE;
        // clang-format off
        if (strncmp(p, "tcp:", 4) && // 4: "tcp:" size
            strncmp(p, "localabstract:", 14) && // 14: "localabstract:" size
            strncmp(p, "localreserved:", 14) && // 14: "localreserved:" size
            strncmp(p, "localfilesystem:", 16) && // 16: "localfilesystem:" size
            strncmp(p, "dev:", 4) && // 4: "dev:" size
            strncmp(p, "jdwp:", 5) && // 5: "jdwp:" size
            strncmp(p, "ark:", 4)) { // 4: "ark:" size
            stringError = "Incorrect forward command";
            outCmd->bJumpDo = true;
        }
        // clang-format on
        outCmd->cmdFlag = CMD_FORWARD_INIT;
        outCmd->parameters = input;
    }
    return stringError;
}

string RunMode(const char *input, FormatCommand *outCmd)
{
    string stringError;
    outCmd->cmdFlag = CMD_UNITY_RUNMODE;
    outCmd->parameters = input + CMDSTR_TARGET_MODE.size() + 1;  // with  ' '
    int portLength = 4;
    int portSpaceLength = 5;
    if (strncmp(outCmd->parameters.c_str(), "port", portLength) &&
        strcmp(outCmd->parameters.c_str(), CMDSTR_TMODE_USB.c_str())) {
        stringError = "Error tmode command";
        outCmd->bJumpDo = true;
    } else if (!strncmp(outCmd->parameters.c_str(), "port ", portSpaceLength)) {
        const char *tmp = input + strlen("tmode port ");
        // command is tmode port close
        if (strcmp(tmp, "close") == 0) {
            return stringError;
        }
        int port = atoi(tmp);
        if (port > MAX_IP_PORT || port <= 0) {
            stringError = "Incorrect port range";
            outCmd->bJumpDo = true;
        }
    }
    return stringError;
}

void TargetReboot(const char *input, FormatCommand *outCmd)
{
    outCmd->cmdFlag = CMD_UNITY_REBOOT;
    if (strcmp(input, CMDSTR_TARGET_REBOOT.c_str())) {
        outCmd->parameters = input + CMDSTR_TARGET_REBOOT.size() + 1;  // with  ' '
        if (outCmd->parameters == "-bootloader" || outCmd->parameters == "-recovery" ||
            outCmd->parameters == "-flashd") {
            outCmd->parameters.erase(outCmd->parameters.begin());
        }
    }
}

// command input
// client side:Enter string data formatting conversion to module see internal processing command
string String2FormatCommand(const char *inputRaw, int sizeInputRaw, FormatCommand *outCmd)
{
    string stringError;
    string input = string(inputRaw, sizeInputRaw);
    if (!strncmp(input.c_str(), CMDSTR_SOFTWARE_HELP.c_str(), CMDSTR_SOFTWARE_HELP.size())) {
        outCmd->cmdFlag = CMD_KERNEL_HELP;
        outCmd->bJumpDo = true;
        if (strstr(input.c_str(), " verbose")) {
            stringError = Verbose();
        } else {
            stringError = Usage();
        }
    } else if (!strcmp(input.c_str(), CMDSTR_SOFTWARE_VERSION.c_str())) {
        outCmd->cmdFlag = CMD_KERNEL_HELP;
        stringError = Base::GetVersion();
        outCmd->bJumpDo = true;
    } else if (!strcmp(input.c_str(), CMDSTR_TARGET_DISCOVER.c_str())) {
        outCmd->cmdFlag = CMD_KERNEL_TARGET_DISCOVER;
    } else if (!strncmp(input.c_str(), CMDSTR_LIST_TARGETS.c_str(), CMDSTR_LIST_TARGETS.size())) {
        outCmd->cmdFlag = CMD_KERNEL_TARGET_LIST;
        if (strstr(input.c_str(), " -v")) {
            outCmd->parameters = "v";
        }
    } else if (!strncmp(input.c_str(), CMDSTR_SERVICE_START.c_str(), CMDSTR_SERVICE_START.size())) {
        outCmd->cmdFlag = CMD_SERVICE_START;
    }
#ifdef HOST_OHOS
    else if (!strncmp(input.c_str(), CMDSTR_SERVICE_KILL.c_str(), CMDSTR_SERVICE_KILL.size())) {
        outCmd->cmdFlag = CMD_SERVER_KILL;
    }
#endif
    else if (!strncmp(input.c_str(), CMDSTR_CHECK_SERVER.c_str(), CMDSTR_CHECK_SERVER.size())) {
        outCmd->cmdFlag = CMD_CHECK_SERVER;
    } else if (!strncmp(input.c_str(), CMDSTR_CHECK_DEVICE.c_str(), CMDSTR_CHECK_DEVICE.size())) {
        outCmd->parameters = input.c_str() + CMDSTR_CHECK_DEVICE.size() + 1;  // with ' '
        outCmd->cmdFlag = CMD_CHECK_DEVICE;
    } else if (!strncmp(input.c_str(), CMDSTR_WAIT_FOR.c_str(), CMDSTR_WAIT_FOR.size())) {
        outCmd->cmdFlag = CMD_WAIT_FOR;
    } else if (!strcmp(input.c_str(), CMDSTR_CONNECT_ANY.c_str())) {
        outCmd->cmdFlag = CMD_KERNEL_TARGET_ANY;
    } else if (!strncmp(input.c_str(), CMDSTR_CONNECT_TARGET.c_str(), CMDSTR_CONNECT_TARGET.size())) {
        outCmd->parameters = input.c_str() + CMDSTR_CONNECT_TARGET.size() + 1;  // with ' '
        stringError = TargetConnect(outCmd);
    } else if (!strncmp(input.c_str(), (CMDSTR_SHELL_EX).c_str(), (CMDSTR_SHELL_EX).size())) {
        if (!HostShellOption::FormatParametersToTlv(
            input, CMDSTR_SHELL_EX.size() - 1, outCmd->parameters, stringError)) {
            outCmd->bJumpDo = true;
        }
        outCmd->cmdFlag = CMD_UNITY_EXECUTE_EX;
    } else if (!strncmp(input.c_str(), (CMDSTR_SHELL + " ").c_str(), CMDSTR_SHELL.size() + 1)) {
        outCmd->cmdFlag = CMD_UNITY_EXECUTE;
        outCmd->parameters = input.c_str() + CMDSTR_SHELL.size() + 1;
    } else if (!strcmp(input.c_str(), CMDSTR_SHELL.c_str())) {
        outCmd->cmdFlag = CMD_SHELL_INIT;
    } else if (!strncmp(input.c_str(), CMDSTR_FILE_SEND.c_str(), CMDSTR_FILE_SEND.size()) ||
                !strncmp(input.c_str(), CMDSTR_FILE_RECV.c_str(), CMDSTR_FILE_RECV.size())) {
        outCmd->cmdFlag = CMD_FILE_INIT;
        outCmd->parameters = input.c_str() + strlen("file ");
    } else if (!strncmp(input.c_str(), string(CMDSTR_FORWARD_FPORT + " ").c_str(), CMDSTR_FORWARD_FPORT.size() + 1)
                || !strncmp(input.c_str(), string(CMDSTR_FORWARD_RPORT + " ").c_str(),
                            CMDSTR_FORWARD_RPORT.size() + 1)) {
        stringError = ForwardPort(input.c_str(), outCmd);
    } else if (!strncmp(input.c_str(), CMDSTR_APP_INSTALL.c_str(), CMDSTR_APP_INSTALL.size())) {
        outCmd->cmdFlag = CMD_APP_INIT;
        outCmd->parameters = input;
    } else if (!strncmp(input.c_str(), CMDSTR_APP_UNINSTALL.c_str(), CMDSTR_APP_UNINSTALL.size())) {
        outCmd->cmdFlag = CMD_APP_UNINSTALL;
        outCmd->parameters = input;
        if (outCmd->parameters.size() > MAX_APP_PARAM_SIZE || outCmd->parameters.size() < MIN_APP_PARAM_SIZE) {
            stringError = "Package's path incorrect";
            outCmd->bJumpDo = true;
        }
    } else if (!strcmp(input.c_str(), CMDSTR_TARGET_MOUNT.c_str())) {
        outCmd->cmdFlag = CMD_UNITY_REMOUNT;
    } else if (!strcmp(input.c_str(), CMDSTR_LIST_JDWP.c_str())) {
        outCmd->cmdFlag = CMD_JDWP_LIST;
    } else if (!strncmp(input.c_str(), CMDSTR_TRACK_JDWP.c_str(), CMDSTR_TRACK_JDWP.size())) {
        outCmd->cmdFlag = CMD_JDWP_TRACK;
        if (strstr(input.c_str(), " -p")) {
            outCmd->parameters = "p";
        } else if (strstr(input.c_str(), " -a")) {
            outCmd->parameters = "a";
        }
    } else if (!strncmp(input.c_str(), CMDSTR_TARGET_REBOOT.c_str(), CMDSTR_TARGET_REBOOT.size())) {
        TargetReboot(input.c_str(), outCmd);
    } else if (!strncmp(input.c_str(), CMDSTR_TARGET_MODE.c_str(), CMDSTR_TARGET_MODE.size())) {
        stringError = RunMode(input.c_str(), outCmd);
    } else if (!strncmp(input.c_str(), CMDSTR_HILOG.c_str(), CMDSTR_HILOG.size())) {
        outCmd->cmdFlag = CMD_UNITY_HILOG;
        if (strstr(input.c_str(), " -h")) {
            outCmd->parameters = "h";
        }
    } else if (!strncmp(input.c_str(), CMDSTR_STARTUP_MODE.c_str(), CMDSTR_STARTUP_MODE.size())) {
        outCmd->cmdFlag = CMD_UNITY_ROOTRUN;
        if (strstr(input.c_str(), " -r")) {
            outCmd->parameters = "r";
        }
    } else if (!strncmp(input.c_str(), CMDSTR_APP_SIDELOAD.c_str(), CMDSTR_APP_SIDELOAD.size())) {
        if (strlen(input.c_str()) == CMDSTR_APP_SIDELOAD.size()) {
            stringError = "Incorrect command, please with local path";
            outCmd->bJumpDo = true;
        }
        outCmd->cmdFlag = CMD_APP_SIDELOAD;
        outCmd->parameters = input;
    } else if (!strncmp(input.c_str(), CMDSTR_BUGREPORT.c_str(), CMDSTR_BUGREPORT.size())) {
        outCmd->cmdFlag = CMD_UNITY_BUGREPORT_INIT;
        outCmd->parameters = input;
        if (outCmd->parameters.size() == CMDSTR_BUGREPORT.size() + 1) { // 1 is sizeInputRaw = input.size() + 1
            outCmd->parameters = CMDSTR_BUGREPORT + " ";
        }
    }
    // Inner command, protocol uses only
    else if (!strncmp(input.c_str(), CMDSTR_INNER_ENABLE_KEEPALIVE.c_str(), CMDSTR_INNER_ENABLE_KEEPALIVE.size())) {
        outCmd->cmdFlag = CMD_KERNEL_ENABLE_KEEPALIVE;
    } else if (HostUpdater::CheckMatchUpdate(input, *outCmd)) {
        outCmd->parameters = input;
    } else {
        stringError = "Unknown command...";
        outCmd->bJumpDo = true;
    }
#ifdef HDC_DEBUG
    WRITE_LOG(LOG_DEBUG, "String2FormatCommand cmdFlag:%d", outCmd->cmdFlag);
#endif
    // nl
    if (stringError.size()) {
        stringError += "\n";
    }
    return stringError;
};
}
}  // namespace Hdc
