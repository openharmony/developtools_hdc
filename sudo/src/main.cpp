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

#define PWD_BUF_LEN 128
#define CHALLENGE_LEN 32
#define DEFAULT_PATH "/system/bin"
#define DEFAULT_BASH "/system/bin/sh"
#define PATH "PATH="
using namespace OHOS::UserIam;
using namespace OHOS::AccountSA;
using namespace OHOS::UserIam::PinAuth;
using namespace OHOS::UserIam::UserAuth;

static FILE *g_ttyFp = nullptr;

static const char *OUT_OF_MEM = "[E0001] out of memory\n";
static const char *COMMAND_NOT_FOUND = "[E0002] command not found\n";
static const char *USER_VERIFY_FAILED = "[E0003] Sorry, try again. If screen lock password not set, set it first.\n";
static const char *HELP = "sudo - execute command as root\n\n"
                       "usage: sudo command ...\n"
                       "usage: sudo sh -c command ...\n";

static int32_t g_userId = -1;
static std::vector<uint8_t> g_challenge(CHALLENGE_LEN, 0);
static std::vector<uint8_t> g_authToken;
static const std::string CONSTRAINT_SUDO = "constraint.sudo";

static void WriteStdErr(const char *str)
{
    (void)fwrite(str, 1, strlen(str), stderr);
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
 * Find cmd from PATH
*/
static bool GetCmdInPath(char *cmd, int cmdBufLen, char *envp[])
{
    struct stat st;
    char *path = nullptr;
    char *pathBak = nullptr;
    char **ep = nullptr;
    char *cp = nullptr;
    char pathBuf[PATH_MAX + 1] = {0};
    bool findSuccess = false;

    if (strchr(cmd, '/') != nullptr) {
        return true;
    }

    for (ep = envp; *ep != nullptr; ep++) {
        if (strncmp(*ep, PATH, strlen(PATH)) == 0) {
            path = *ep + strlen(PATH);
            break;
        }
    }

    path = StrDup((path != nullptr && *path != '\0') ? path : DEFAULT_PATH);
    pathBak = path;
    do {
        if ((cp = strchr(path, ':')) != nullptr) {
            *cp = '\0';
        }
        int ret = sprintf_s(pathBuf, sizeof(pathBuf), "%s/%s", *path ? path : ".", cmd);
        if (ret > 0 && stat(pathBuf, &st) == 0 && S_ISREG(st.st_mode)) {
            findSuccess = true;
            break;
        }
        path = cp + 1;
    } while (cp != nullptr);

    delete [] pathBak;
    if (!findSuccess) {
        WriteTty(COMMAND_NOT_FOUND);
        return false;
    }
    return (sprintf_s(cmd, cmdBufLen, "%s", pathBuf) < 0) ? false : true;
}

static char **ParseCmd(int argc, char* argv[], char* env[], char *cmd, int cmdLen)
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
        if (ret < 0 || !GetCmdInPath(cmd, cmdLen, env)) {
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

static void GetUserPwd(char *pwdBuf, int bufLen)
{
    const char *prompts = "[sudo] password for current user:";
    const char *newline = "\n";
    struct termios oldTerm;
    struct termios newTerm;

    WriteTty(prompts);

    tcgetattr(STDIN_FILENO, &oldTerm);
    newTerm = oldTerm;
    newTerm.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTerm);
    (void)fgets(pwdBuf, bufLen, stdin);
    if (strlen(pwdBuf) != 0 && pwdBuf[strlen(pwdBuf) - 1] == '\n') {
        pwdBuf[strlen(pwdBuf) - 1] = '\0';
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
    
    WriteTty(newline);
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

static bool VerifyAccount()
{
    bool verifyResult = false;

    UserAuthClient &sudoIAMClient = UserAuthClient::GetInstance();
    std::shared_ptr<UserAuth::AuthenticationCallback> callback = std::make_shared<SudoIDMCallback>();

    OHOS::UserIam::UserAuth::AuthParam authParam;
    authParam.userId = g_userId;
    authParam.challenge = g_challenge;
    authParam.authType = AuthType::PIN;
    authParam.authTrustLevel = AuthTrustLevel::ATL1;

    sudoIAMClient.BeginAuthentication(authParam, callback);
    std::shared_ptr<SudoIDMCallback> sudoCallback = std::static_pointer_cast<SudoIDMCallback>(callback);
    WaitForAuth();
    verifyResult = sudoCallback->GetVerifyResult();
    g_authToken = sudoCallback->GetAuthToken();
    return verifyResult;
}

static bool GetUserId()
{
    std::vector<int32_t> ids;

    OHOS::ErrCode err = OsAccountManager::QueryActiveOsAccountIds(ids);
    if (err != 0) {
        WriteStdErr("get os account local id failed\n");
        return false;
    }

    g_userId = ids[0];
    return true;
}

static bool UserAccountVerify(char *pwd, int pwdLen)
{
    bool verifyResult = false;
    std::shared_ptr<PinAuth::IInputer> inputer = nullptr;

    inputer = std::make_shared<PinAuth::SudoIInputer>();
    std::shared_ptr<PinAuth::SudoIInputer> sudoInputer = std::static_pointer_cast<PinAuth::SudoIInputer>(inputer);
    sudoInputer->SetPasswd(pwd, pwdLen);
    if (!PinAuthRegister::GetInstance().RegisterInputer(inputer)) {
        WriteStdErr("register pin inputer failed\n");
        return false;
    }

    if (VerifyAccount()) {
        verifyResult = true;
    }

    PinAuthRegister::GetInstance().UnRegisterInputer();
    return verifyResult;
}

static bool VerifyUserPin()
{
    char passwd[PWD_BUF_LEN] = {0};
    bool pwdVerifyResult = false;

    if (getuid() == 0) {
        return true;
    }

    GetUserPwd(passwd, PWD_BUF_LEN);
    if (strlen(passwd) == 0) {
        WriteTty(USER_VERIFY_FAILED);
        return pwdVerifyResult;
    }

    pwdVerifyResult = UserAccountVerify(passwd, strnlen(passwd, PWD_BUF_LEN));
    memset_s(passwd, sizeof(passwd), 0, sizeof(passwd));
    if (!pwdVerifyResult) {
        WriteTty(USER_VERIFY_FAILED);
    }
    return pwdVerifyResult;
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

int main(int argc, char* argv[], char* env[])
{
    if (!GetUserId()) {
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
    char **argvNew = ParseCmd(argc, argv, env, execCmd, PATH_MAX + 1);
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
