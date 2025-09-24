/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#include <termios.h>
#include <cstring>
#include <unistd.h>
#include <climits>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <securec.h>

#if defined(SURPPORT_SELINUX)
#include "selinux/selinux.h"
#endif
#include "os_account_manager.h"
#include "sudo_iam.h"
#include "user_auth_client.h"
#include "pinauth_register.h"
#include "user_access_ctrl_client.h"
#include "aclmgr_system_api.h"

using namespace OHOS::UserIam;
using namespace OHOS::AccountSA;
using namespace OHOS::UserIam::UserAuth;

namespace {
static const int CHALLENGE_LEN = 32;
static const std::vector<std::string> DEFAULT_PATH = {"/bin", "/usr/bin", "/system/bin", "/vendor/bin",
    "/usr/local/bin", "/data/app/bin", "/data/service/hnp/bin", "/opt/homebrew"};
static const char *DEFAULT_BASH = "/system/bin/sh";
static FILE *g_ttyFp = nullptr;
static const char *OUT_OF_MEM = "[E0001] out of memory\n";
static const char *COMMAND_NOT_FOUND = "[E0002] command not found\n";
static const char *USER_VERIFY_FAILED = "[E0003] Sorry, try again. If screen lock password not set, set it first.\n";
static const char *USER_SWITCH_FAILED = "[E0004] Currently logged-in user has switched, please try again.\n";
static const char *HELP = "sudo - execute command as root\n\n"
                       "usage: sudo command ...\n"
                       "usage: sudo sh -c command ...\n";
static int32_t g_userId = -1;
static std::vector<uint8_t> g_challenge(CHALLENGE_LEN, 0);
static std::vector<uint8_t> g_authToken;
static const std::string CONSTRAINT_SUDO = "constraint.sudo";
static const std::string TITLE = "Allow execution of sudo commands";
}

static void WriteStdErr(const char *str)
{
    (void)fwrite(str, 1, strlen(str), stderr);
    fflush(stderr);
}

static void WriteStdErrFmtWithStr(const std::string& format, const std::string& error)
{
    fprintf(stderr, format.c_str(), error.c_str());
    fflush(stderr);
}

static void WriteTty(const char *str)
{
    if (g_ttyFp != nullptr) {
        (void)fwrite(str, 1, strlen(str), g_ttyFp);
        fflush(g_ttyFp);
    } else {
        g_ttyFp = fopen("/dev/tty", "w");
        if (g_ttyFp != nullptr) {
            (void)fwrite(str, 1, strlen(str), g_ttyFp);
            fflush(g_ttyFp);
            return;
        }
        WriteStdErr("open /dev/tty for write failed\n");
    }
}

static void CloseTty(void)
{
    if (g_ttyFp != nullptr) {
        fclose(g_ttyFp);
    }
    g_ttyFp = nullptr;
}

static char *StrDup(const char *str)
{
    int ret;
    char *result = new(std::nothrow)char[strlen(str) + 1];
    if (result == nullptr) {
        WriteStdErr(OUT_OF_MEM);
        exit(1);
    }
    ret = strcpy_s(result, strlen(str) + 1, str);
    if (ret != 0) {
        WriteStdErr(OUT_OF_MEM);
        exit(1);
    }
    return result;
}

static void FreeArgvNew(char **argvNew)
{
    char **p = nullptr;
    for (p = argvNew; *p != nullptr; p++) {
        delete [] *p;
    }
    delete [] argvNew;
}

/*
 * Find cmd from DEFAULT_PATH
*/
static bool GetCmdInPath(char *cmd, int cmdBufLen)
{
    std::string cmdStr(cmd);
    struct stat st;

    if (cmdStr.find('/') != std::string::npos) {
        if (stat(cmdStr.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            return true;
        } else {
            WriteTty(COMMAND_NOT_FOUND);
            return false;
        }
    }

    for (const auto &path : DEFAULT_PATH) {
        std::string cmdPath = path.empty() ? cmd : path + "/" + cmd;
        if (stat(cmdPath.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            size_t len = cmdPath.length();
            int ret = memcpy_s(cmd, cmdBufLen - 1, cmdPath.c_str(), len);
            if (ret != EOK) {
                WriteTty(COMMAND_NOT_FOUND);
                return false;
            }
            return true;
        }
    }

    WriteTty(COMMAND_NOT_FOUND);
    return false;
}

static char **ParseCmd(int argc, char* argv[], char *cmd, int cmdLen)
{
    int startCopyArgvIndex = 1;
    int argvNewIndex = 0;
    char **argvTmp = nullptr;
    bool isShc = false;
    int ret;

    /*
     * Here, we construct the command and its argv
     * sudo sh -c xxx yyy -----> sh -c xxx yyy
     * sudo xxx yyy       -----> xxx yyy
    */
    if (argc <= 0) {
        return nullptr;
    }
    argvTmp = new(std::nothrow) char* [argc];
    if (argvTmp == nullptr) {
        WriteStdErr(OUT_OF_MEM);
        return nullptr;
    }
    (void)memset_s(argvTmp, sizeof(char*) * argc, 0, sizeof(char*) * argc);
    /*
     * sudo sh -c xxxx
    */
    if (argc >= 3) { //3:argc of main
        if (strcmp(argv[1], "sh") == 0 && strcmp(argv[2], "-c") == 0) { //2:argv 2 of main
            // argvNew[0] is "/system/bin/sh"
            argvTmp[argvNewIndex++] = StrDup(DEFAULT_BASH);
            // argvNew[1] is "-c"
            argvTmp[argvNewIndex++] = StrDup("-c");
            ret = sprintf_s(cmd, cmdLen, "%s", DEFAULT_BASH);
            if (ret < 0) {
                FreeArgvNew(argvTmp);
                return nullptr;
            }
            startCopyArgvIndex = 3; //3:start copy index of argv
            isShc = true;
        }
    }

    /*
     * if not "sudo sh -c xxxx", just as "sudo xxxx"
    */
    if (!isShc) {
        ret = sprintf_s(cmd, cmdLen, "%s", argv[1]);
        if (ret < 0 || !GetCmdInPath(cmd, cmdLen)) {
            FreeArgvNew(argvTmp);
            return nullptr;
        }
        argvTmp[argvNewIndex++] = StrDup(cmd);
        startCopyArgvIndex = 2; //2:start copy index of argv
    }

    for (int i = startCopyArgvIndex; i < argc; i++) {
        argvTmp[argvNewIndex++] = StrDup(argv[i]);
    }
    argvTmp[argvNewIndex] = nullptr;
    
    return argvTmp;
}

static bool SetUidGid(void)
{
    if (setuid(0) != 0) {
        return false;
    }
    if (setegid(0) != 0) {
        return false;
    }
    if (setgid(0) != 0) {
        return false;
    }
    return true;
}

static void WaitForAuth(void)
{
    std::unique_lock<std::mutex> lock(g_mutexForAuth);
    g_condVarForAuth.wait(lock, [] { return g_authFinish; });
}

static bool GetChallenge()
{
    int32_t res = InitChallengeForCommand(g_challenge.data(), g_challenge.size());
    if (res != 0) {
        WriteStdErr("init challenge failed\n");
        return false;
    }
    return true;
}

static int32_t GetUserId()
{
    std::vector<int32_t> ids;

    OHOS::ErrCode err = OsAccountManager::QueryActiveOsAccountIds(ids);
    if (err != 0) {
        WriteStdErr("get os account local id failed\n");
        return -1;
    }
    return ids[0];
}

static bool VerifyUserPin()
{
    if (getuid() == 0) {
        return true;
    }

    int curUserId = GetUserId();
    // switch user when process running, quick cancel.
    if (curUserId != g_userId) {
        WriteTty(USER_SWITCH_FAILED);
        return false;
    }

    UserAuthClient &sudoIAMClient = UserAuthClient::GetInstance();
    std::shared_ptr<SudoIDMCallback> callback = std::make_shared<SudoIDMCallback>();

    OHOS::UserIam::UserAuth::WidgetAuthParam widgetAuthParam;
    widgetAuthParam.userId = g_userId;
    widgetAuthParam.challenge = g_challenge;
    widgetAuthParam.authTrustLevel = AuthTrustLevel::ATL3;
    widgetAuthParam.authTypes = { AuthType::PIN };

    OHOS::UserIam::UserAuth::WidgetParam widgetParam;
    widgetParam.title = TITLE;
    widgetParam.windowMode = OHOS::UserIam::UserAuth::WindowModeType::UNKNOWN_WINDOW_MODE;

    sudoIAMClient.BeginWidgetAuth(widgetAuthParam, widgetParam, callback);
    WaitForAuth();
    bool verifyResult = callback->GetVerifyResult();
    g_authToken = callback->GetAuthToken();
    if (!verifyResult) {
        WriteTty(USER_VERIFY_FAILED);
    }
    return verifyResult;
}

static bool SetPSL()
{
    int32_t res = SetProcessLevelByCommand(g_authToken.data(), g_authToken.size());
    if (res != 0) {
        WriteStdErr("set psl failed\n");
        return false;
    }
    return true;
}

#if defined(SURPPORT_ACCOUNT_CONSTRAINT)
static bool CheckUserLimitation()
{
    bool isNotEnabled = false;
    OHOS::ErrCode err = OsAccountManager::CheckOsAccountConstraintEnabled(
        g_userId, CONSTRAINT_SUDO, isNotEnabled);
    if (err != 0) {
        WriteStdErr("check account constrain failed.\n");
        return false;
    }

    if (isNotEnabled) {
        WriteStdErr("no permision.\n");
        return false;
    }

    return true;
}
#endif

static bool UpdateEnvironmentPath()
{
    if (unsetenv("PATH") != 0) {
        WriteStdErrFmtWithStr("unsetenv failed!error message:%s\n", std::strerror(errno));
        return false;
    }

    std::string newPath;
    for (const auto &pathDir : DEFAULT_PATH) {
        if (!newPath.empty()) {
            newPath += ":";
        }
        newPath += pathDir;
    }

    if (!newPath.empty() && setenv("PATH", newPath.c_str(), 1) != 0) {
        WriteStdErrFmtWithStr("setenv failed!error message:%s\n", std::strerror(errno));
        return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    if (!UpdateEnvironmentPath()) {
        return 1;
    }

    g_userId = GetUserId();
    if (g_userId == -1) {
        WriteStdErr("get user id failed.\n");
        return 1;
    }

#if defined(SURPPORT_ACCOUNT_CONSTRAINT)
    if (!CheckUserLimitation()) {
        return 1;
    }
#endif

    if (argc < 2) { // 2:argc check number
        WriteStdErr(HELP);
        return 1;
    }

    if (!GetChallenge()) {
        return 1;
    }

    if (!VerifyUserPin()) {
        return 1;
    }

    if (!SetPSL()) {
        return 1;
    }

    char execCmd[PATH_MAX + 1] = {0};
    char **argvNew = ParseCmd(argc, argv, execCmd, PATH_MAX + 1);
    CloseTty();
    if (argvNew == nullptr) {
        return 1;
    }

    if (!SetUidGid()) {
        FreeArgvNew(argvNew);
        WriteStdErr("setuid failed\n");
        return 1;
    }

#if defined(SURPPORT_SELINUX)
    if (setcon("u:r:sudo_execv_label:s0") != 0) {
        WriteStdErr("set SEHarmony label failed\n");
        return 1;
    }
#endif

    execvp(execCmd, argvNew);
    WriteStdErr("execvp failed\n");
    return 1;
}
