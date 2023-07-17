/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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
#include "ext_client.h"
#include "libgen.h"
#include "common.h"

namespace Hdc {
ExtClient::ExtClient()
{
    lib.handle = nullptr;
}

ExtClient::~ExtClient()
{
    if (lib.handle != nullptr) {
        uv_dlclose(&lib);
    }
}

string ExtClient::GetPath()
{
#ifdef _WIN32
    string path = "libexternal_hdc.dll";
#elif defined(HOST_MAC)
    string path = "libexternal_hdc.dylib";
#else
    string path = "libexternal_hdc.z.so";
#endif
    string hdcPath = Base::GetHdcAbsolutePath();
    int index = hdcPath.find_last_of(Base::GetPathSep());
    return (hdcPath.substr(0, index) + Base::GetPathSep() + path);
}

bool ExtClient::Init()
{
    string path = GetPath();
    int rc = uv_dlopen(path.c_str(), &lib);
    if (rc != 0) {
        WRITE_LOG(LOG_FATAL, "uv_dlopen failed %s %s", path.c_str(), uv_dlerror(&lib));
        return false;
    }
    RegistExecFunc(&lib);
    return true;
}

bool ExtClient::SharedLibraryExist()
{
    string path = GetPath();
    return Base::CheckDirectoryOrPath(path.c_str(), true, true);
}

void ExtClient::ExecuteCommand(const string &command)
{
    if (!strncmp(command.c_str(), CMDSTR_SOFTWARE_VERSION.c_str(), CMDSTR_SOFTWARE_VERSION.size())) {
        Version(command);
    } else if (!strncmp(command.c_str(), CMDSTR_SOFTWARE_HELP.c_str(), CMDSTR_SOFTWARE_HELP.size())) {
        Help(command);
    } else if (!strncmp(command.c_str(), CMDSTR_TARGET_DISCOVER.c_str(), CMDSTR_TARGET_DISCOVER.size())) {
        Discover(command);
    } else if (!strncmp(command.c_str(), CMDSTR_SERVICE_START.c_str(), CMDSTR_SERVICE_START.size())) {
        Start(command);
    } else if (!strncmp(command.c_str(), CMDSTR_SERVICE_KILL.c_str(), CMDSTR_SERVICE_KILL.size())) {
        Kill(command);
    } else if (!strncmp(command.c_str(), CMDSTR_CONNECT_TARGET.c_str(), CMDSTR_CONNECT_TARGET.size())) {
        Connect(command);
    } else if (!strncmp(command.c_str(), CMDSTR_LIST_TARGETS.c_str(), CMDSTR_LIST_TARGETS.size())) {
        ListTargets(command);
    } else if (!strncmp(command.c_str(), CMDSTR_SHELL.c_str(), CMDSTR_SHELL.size())) {
        Shell(command);
    } else if (!strncmp(command.c_str(), CMDSTR_FILE_SEND.c_str(), CMDSTR_FILE_SEND.size()) ||
               !strncmp(command.c_str(), CMDSTR_FILE_RECV.c_str(), CMDSTR_FILE_RECV.size())) {
        File(command);
    } else if (!strncmp(command.c_str(), CMDSTR_APP_INSTALL.c_str(), CMDSTR_APP_INSTALL.size())) {
        Install(command);
    } else if (!strncmp(command.c_str(), CMDSTR_APP_UNINSTALL.c_str(), CMDSTR_APP_UNINSTALL.size())) {
        Uninstall(command);
    } else if (!strncmp(command.c_str(), CMDSTR_FORWARD_FPORT.c_str(), CMDSTR_FORWARD_FPORT.size())) {
        Fport(command);
    } else if (!strncmp(command.c_str(), CMDSTR_FORWARD_RPORT.c_str(), CMDSTR_FORWARD_RPORT.size())) {
        Rport(command);
    } else if (!strncmp(command.c_str(), CMDSTR_LIST_JDWP.c_str(), CMDSTR_LIST_JDWP.size())) {
        Jpid(command);
    } else if (!strncmp(command.c_str(), CMDSTR_TRACK_JDWP.c_str(), CMDSTR_TRACK_JDWP.size())) {
        TrackJpid(command);
    } else if (!strncmp(command.c_str(), (CMDSTR_SHELL + " ").c_str(), CMDSTR_SHELL.size() + 1) ||
               !strncmp(command.c_str(), CMDSTR_TARGET_REBOOT.c_str(), CMDSTR_TARGET_REBOOT.size()) ||
               !strncmp(command.c_str(), CMDSTR_TARGET_MOUNT.c_str(), CMDSTR_TARGET_MOUNT.size()) ||
               !strncmp(command.c_str(), CMDSTR_STARTUP_MODE.c_str(), CMDSTR_STARTUP_MODE.size()) ||
               !strncmp(command.c_str(), CMDSTR_TARGET_MODE.c_str(), CMDSTR_TARGET_MODE.size()) ||
               !strncmp(command.c_str(), CMDSTR_HILOG.c_str(), CMDSTR_HILOG.size())) {
        Utility(command);
    } else if (!strncmp(command.c_str(), CMDSTR_BUGREPORT.c_str(), CMDSTR_BUGREPORT.size())) {
        Bugreport(command);
    } else if (!strncmp(command.c_str(), CMDSTR_WAIT_FOR.c_str(), CMDSTR_WAIT_FOR.size())) {
        WaitFor(command);
    } else {
        UnknowCommand(command);
    }
}

void ExtClient::Version(const std::string &str)
{
    const char *name = "HdcExtVersion";
    Handle(str, name);
}

void ExtClient::Help(const std::string &str)
{
    return;
}

void ExtClient::Discover(const std::string &str)
{
    const char *name = "HdcExtDiscover";
    Handle(str, name);
}

void ExtClient::Start(const std::string &str)
{
    const char *name = "HdcExtStart";
    Handle(str, name);
}

void ExtClient::Kill(const std::string &str)
{
    const char *name = "HdcExtKill";
    Handle(str, name);
}

void ExtClient::Connect(const std::string &str)
{
    const char *name = "HdcExtConnect";
    string res = Handle(str, name);
    if (res.find("connected to") != std::string::npos) {
        _exit(0);
    }
}

void ExtClient::ListTargets(const std::string &str)
{
    typedef void (*HdcExtListTargets)(const char *, uint64_t, char *, uint64_t &);
    const char *name = "HdcExtListTargets";
    HdcExtListTargets listTargets;
    int rc = uv_dlsym(&lib, name, (void **) &listTargets);
    if (rc != 0) {
        WRITE_LOG(LOG_FATAL, "uv_dlsym %s failed %s", name, uv_dlerror(&lib));
    } else {
        uint64_t size = 4096;
        char *buffer = new(std::nothrow) char[size]();
        if (buffer == nullptr) {
            WRITE_LOG(LOG_FATAL, "new buffer failed with function %s", name);
            return;
        }
        listTargets(str.c_str(), str.size(), buffer, size);
        string extdevs(buffer);
        UpdateList(extdevs);
        delete[] buffer;
        if (extdevs.empty()) {
            return;
        }
        if (g_show) {
            const string listv = "list targets -v";
            if (!strncmp(str.c_str(), listv.c_str(), listv.size())) {
                string all = extdevs;
                all = Base::ReplaceAll(all, "\n", "\texternal\n");
                Base::PrintMessage("%s", all.c_str());
            } else {
                Base::PrintMessage("%s", extdevs.c_str());
            }
        }
    }
}

void ExtClient::UpdateList(const string &str)
{
    if (str.empty()) {
        return;
    }
    vector<string> devs;
    Base::SplitString(str, "\n", devs);
    for (size_t i = 0; i < devs.size(); i++) {
        string::size_type pos = devs[i].find("\t");
        if (pos != string::npos || (pos = devs[i].find(" ")) != string::npos) {
            string key = devs[i].substr(0, pos);
            g_lists[key] = "external";
        }
    }
}

void ExtClient::Shell(const std::string &str)
{
    const char *name = "HdcExtShell";
    string value = WithConnectKey(str);
    Handle(value, name);
}

void ExtClient::File(const std::string &str)
{
    const char *name = "HdcExtFile";
    std::string cmd = RemoveRemoteCwd(str);
    string value = WithConnectKey(cmd);
    Handle(value, name);
}

void ExtClient::Install(const std::string &str)
{
    const char *name = "HdcExtInstall";
    std::string cmd = RemoveRemoteCwd(str);
    string value = WithConnectKey(cmd);
    Handle(value, name);
}

void ExtClient::Uninstall(const std::string &str)
{
    const char *name = "HdcExtUninstall";
    string value = WithConnectKey(str);
    Handle(value, name);
}

void ExtClient::Fport(const std::string &str)
{
    const char *name = "HdcExtFport";
    string value = WithConnectKey(str);
    Handle(value, name);
}

void ExtClient::Rport(const std::string &str)
{
    const char *name = "HdcExtRport";
    string value = WithConnectKey(str);
    Handle(value, name);
}

void ExtClient::Jpid(const std::string &str)
{
    const char *name = "HdcExtJpid";
    string value = WithConnectKey(str);
    Handle(value, name);
}

void ExtClient::TrackJpid(const std::string &str)
{
    const char *name = "HdcExtTrackJpid";
    string value = WithConnectKey(str);
    Handle(value, name);
}

void ExtClient::Utility(const std::string &str)
{
    const char *name = "HdcExtUtility";
    string value = WithConnectKey(str);
    Handle(value, name);
}

void ExtClient::Bugreport(const std::string &str)
{
    const char *name = "HdcExtBugreport";
    string value = WithConnectKey(str);
    Handle(value, name);
}

void ExtClient::WaitFor(const std::string &str)
{
    std::thread([str]() {
        WaitForExtent(str);
        _exit(0);
    }).detach();
}

void ExtClient::UnknowCommand(const std::string &str)
{
    const char *name = "HdcExtUnknowCommand";
    Handle(str, name);
}

std::string ExtClient::RemoveRemoteCwd(const std::string &str)
{
    int argc = 0;
    std::string cmd = str;
    char **argv = Base::SplitCommandToArgs(cmd.c_str(), &argc);
    if (argv == nullptr) {
        return cmd;
    }
    for (int i = 0; i < argc; i++) {
        if (argv[i] == CMDSTR_REMOTE_PARAMETER) {
            std::string remove = Base::StringFormat("%s %s \"%s\" ", argv[i], argv[i + 1], argv[i + 2]);
            if (cmd.find(remove) != std::string::npos) {
                cmd.replace(cmd.find(remove), remove.size(), "");
            }
            break;
        }
    }
    delete[](reinterpret_cast<char *>(argv));
    return cmd;
}

std::string ExtClient::HandleLib(const std::string &str, const char *name, uv_lib_t &lib)
{
    typedef void (*HdcExtCommand)(const char *, uint64_t, char *, uint64_t &);
    HdcExtCommand command;
    std::string strBuf;
    int rc = uv_dlsym(&lib, name, (void **) &command);
    if (rc != 0) {
        WRITE_LOG(LOG_FATAL, "uv_dlsym %s failed %s", name, uv_dlerror(&lib));
    } else {
        uint64_t size = 4096;
        char *buffer = new(std::nothrow) char[size]();
        if (buffer == nullptr) {
            WRITE_LOG(LOG_FATAL, "new buffer failed with function %s", name);
            return "";
        }
        command(str.c_str(), str.size(), buffer, size);
        strBuf = buffer;
        if (!strBuf.empty()) {
            Base::PrintMessage("%s", strBuf.c_str());
        }
        delete[] buffer;
    }
    return strBuf;
}

std::string ExtClient::Handle(const std::string &str, const char *name)
{
    return HandleLib(str, name, this->lib);
}

std::string ExtClient::WithConnectKey(const string &str)
{
    std::string value;
    if (!connectKey.empty()) {
        value = "-t " + connectKey + " ";
    }
    if (!containerInOut.empty()) {
        value = value + containerInOut + " ";
    }
    value = value + str;
    return value;
}

void ExtClient::WaitForExtent(const std::string &str)
{
    uv_lib_t uvLib;
    string path = GetPath();
    int rc = uv_dlopen(path.c_str(), &uvLib);
    if (rc != 0) {
        WRITE_LOG(LOG_FATAL, "uv_dlopen failed %s %s", path.c_str(), uv_dlerror(&uvLib));
        return;
    }
    RegistExecFunc(&uvLib);
    const char *name = "HdcExtWaitFor";
    HandleLib(str, name, uvLib);
    uv_dlclose(&uvLib);
}

static void OnExit(uv_process_t *req, int64_t exitStatus, int termSignal)
{
    uv_close((uv_handle_t*) req, nullptr);
}

static int ExternalExecFunc(int argc, char *argv[])
{
#define EXTERNAL_KEEP_FDS 3
    uv_loop_t loop;
    uv_process_t childReq = { 0 };
    uv_process_options_t options = { 0 };
    uv_stdio_container_t childStdio[EXTERNAL_KEEP_FDS];

    if (argc <= 0) {
        return 1;
    }

    uv_loop_init(&loop);

    for (int i = 0; i < EXTERNAL_KEEP_FDS; i++) {
        childStdio[i].flags = UV_INHERIT_FD;
        childStdio[i].data.fd = i;
    }

    size_t pathSize = BUF_SIZE_DEFAULT4;
    char execPath[pathSize];
    (void)memset_s(execPath, pathSize, 0, pathSize);
    int ret = uv_exepath(execPath, &pathSize);
    if (ret < 0) {
        return 1;
    }
    string path = string(dirname(execPath)) + "/" + string(argv[0]);
    options.file = path.c_str();
    options.args = argv;
    options.exit_cb = OnExit;
    options.stdio_count = EXTERNAL_KEEP_FDS;
    options.stdio = childStdio;

    if (uv_spawn(&loop, &childReq, &options) != 0) {
        return 1;
    }
    uv_run(&loop, UV_RUN_DEFAULT);

#ifdef HOST_MINGW
    DWORD status = 0;
    if (GetExitCodeProcess(childReq.process_handle, &status)) {
        return uv_translate_sys_error(GetLastError());
    }
#else
    int status = 0;
    if (!WIFEXITED(childReq.status)) {
        return errno;
    }
    status = WEXITSTATUS(childReq.status);
#endif
    return static_cast<int>(status);
}

void ExtClient::RegistExecFunc(uv_lib_t *lib)
{
    typedef void (*HdcExtRegistExec)(int *);
    HdcExtRegistExec registExec;
    const char *name = "HdcExtRegistExecFunc";
    int rc = uv_dlsym(lib, name, (void **) &registExec);
    if (rc == 0) {
        registExec(reinterpret_cast<int *>(ExternalExecFunc));
    }
}
}
