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
#include <unistd.h>

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
static const int SUDO_ARG_MAX_NUMS = 4096;
static const int CHALLENGE_LEN = 32;
static const std::vector<std::string> DEFAULT_PATH = {"/bin", "/system/usr/bin", "/system/bin", "/vendor/bin",
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
static const int USER_NO_PREMISSION = 25000008; // InitChallengeForCommand return current user no permission

static std::vector<std::string> envSnapshot;

/*
 * Default table of "bad" variables to remove from the environment.
 */
static std::vector<const std::string> initialBadenvTables = {
    "IFS",
    "CDPATH",
    "LOCALDOMAIN",
    "RES_OPTIONS",
    "HOSTALIASES",
    "NLSPATH",
    "PATH_LOCALE",
    "LD_*",
    "_RLD*",
    "SHLIB_PATH",
    "LDR_*",
    "LIBPATH",
    "AUTHSTATE",
    "DYLD_*",
    "KRB5_CONFIG*",
    "KRB5_KTNAME",
    "VAR_ACE",
    "USR_ACE",
    "DLC_ACE",
    "TERMINFO",             /* terminfo, exclusive path to terminfo files */
    "TERMINFO_DIRS",        /* terminfo, path(s) to terminfo files */
    "TERMPATH",             /* termcap, path(s) to termcap files */
    "TERMCAP",              /* XXX - only if it starts with '/' */
    "ENV",                  /* ksh, file to source before script runs */
    "BASH_ENV",             /* bash, file to source before script runs */
    "PS4",                  /* bash, prefix for lines in xtrace mode */
    "GLOBIGNORE",           /* bash, globbing patterns to ignore */
    "BASHOPTS",             /* bash, initial "shopt -s" options */
    "SHELLOPTS",            /* bash, initial "set -o" options */
    "JAVA_TOOL_OPTIONS",    /* java, extra command line options */
    "PERLIO_DEBUG",         /* perl, debugging output file */
    "PERLLIB",              /* perl, search path for modules/includes */
    "PERL5LIB",             /* perl 5, search path for modules/includes */
    "PERL5OPT",             /* perl 5, extra command line options */
    "PERL5DB",              /* perl 5, command used to load debugger */
    "FPATH",                /* ksh, search path for functions */
    "NULLCMD",              /* zsh, command for null file redirection */
    "READNULLCMD",          /* zsh, command for null file redirection */
    "ZDOTDIR",              /* zsh, search path for dot files */
    "TMPPREFIX",            /* zsh, prefix for temporary files */
    "PYTHONHOME",           /* python, module search path */
    "PYTHONPATH",           /* python, search path */
    "PYTHONINSPECT",        /* python, allow inspection */
    "PYTHONUSERBASE",       /* python, per user site-packages directory */
    "RUBYLIB",              /* ruby, library load path */
    "RUBYOPT",              /* ruby, extra command line options */
    "*=()*"                 /* bash functions */
};

/*
 * Default table of variables to check for '%' and '/' characters.
 */
static std::vector<std::string> initialCheckenvTables = {
    "COLORTERM",
    "LANG",
    "LANGUAGE",
    "LC_*",
    "LINGUAS",
    "TERM",
    "TZ"
};

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
    if ((argc <= 0) || (argc > SUDO_ARG_MAX_NUMS)) {
        return nullptr;
    }
    argvTmp = new(std::nothrow) char* [argc + 1]; //+1:for nullptr
    if (argvTmp == nullptr) {
        WriteStdErr(OUT_OF_MEM);
        return nullptr;
    }
    (void)memset_s(argvTmp, sizeof(char*) * (argc + 1), 0, sizeof(char*) * (argc + 1));
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
    if (g_userId == -1) {
        WriteStdErr("GetChallenge userid is failed!\n");
        return false;
    }
    int32_t res = InitChallengeForCommand(g_userId, g_challenge.data(), g_challenge.size());
    if (res != 0) {
        if (res == USER_NO_PREMISSION) {
            WriteStdErr("no permission.\n");
        } else {
            WriteStdErr("init challenge failed\n");
        }
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
    if (ids.empty()) {
        WriteStdErr("os account return is empty!\n");
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

static void SaveEnvSnapshot()
{
    for (int i = 0; environ[i] != nullptr; ++i) {
        envSnapshot.push_back(environ[i]);
    }
}

static bool MatchWithWildcard(const std::string& pattern, const std::string& value)
{
    size_t pIdx = 0;
    size_t vIdx = 0;
    size_t lastStar = std::string::npos;
    size_t lastMatch = 0;
    bool sawSep = false;

    size_t patternSize = pattern.size();
    size_t valueSize = value.size();

    while ((pIdx < patternSize && vIdx <= valueSize)) {
        char pChar = (pIdx < patternSize) ? pattern[pIdx] : '\0';
        char vChar = (vIdx < valueSize) ? value[vIdx] : '\0';

        if (pChar == '*') { // 跳过连续的 '*'
            while (pIdx < patternSize && pattern[pIdx] == '*') {
                pIdx++;
            }

            if (pIdx >= patternSize) { // '*'在pattern末尾，匹配任意内容
                return true;
            }

            lastStar = pIdx - 1;
            lastMatch = vIdx;

            if (((pIdx < patternSize) && (pattern[pIdx] == '='))) {
                sawSep = true;
            }
            continue;
        }
        if (pChar == vChar) {
            if (pChar == '=') {
                sawSep = true;
            }
            pIdx++;
            vIdx++;
        } else if (lastStar != std::string::npos) {
            pIdx = lastStar + 1;
            lastMatch++;
            vIdx = lastMatch;

            if (vIdx < valueSize) {
                char nextPatternChar = (pIdx < patternSize) ? pattern[pIdx] : '\0';
                if (nextPatternChar != '\0') {
                    while ((sawSep) && (vIdx < valueSize) && (value[vIdx] != nextPatternChar)) {
                        vIdx++;
                    }
                }
            }
        } else {
            return false;
        }
    }
    while ((pIdx < patternSize) && pattern[pIdx] == '*') {
        pIdx++;
    }
    return pIdx >= patternSize && vIdx >= valueSize;
}

static bool MatchesEnvPattern(const std::string& value, const std::string& pattern)
{
    size_t startPos = pattern.find('*');
    bool isWild = (startPos != std::string::npos);
    size_t len = (isWild) ? startPos : pattern.size();

    bool match = false;

    if (isWild) {
        match = MatchWithWildcard(pattern, value);
    } else {
        if (strncmp(pattern.c_str(), value.c_str(), len) == 0) {
            match = true;
        }
    }

    return match;
}

static bool MatchesEnvDel(const std::string& envItem)
{
    for (const auto& badItem : initialBadenvTables) {
        if (!badItem.empty()) {
            bool result = MatchesEnvPattern(envItem, badItem);
            if (result) {
                return result;
            }
        }
    }
    return false;
}

static bool TzIsSafe(std::string tzItem)
{
    if (!tzItem.empty() && tzItem[0] == ':') {
        tzItem = tzItem.substr(1);
    }

    if (!tzItem.empty() && tzItem[0] == '/') {
        return false;
    }

    char lastCh = '/';
    for (size_t i = 0; i < tzItem.size(); ++i) {
        char ch = tzItem[i];
        if (std::isspace(static_cast<unsigned char>(ch)) || !std::isprint(static_cast<unsigned char>(ch))) {
            return false;
        }
        if ((lastCh == '/') &&
            (ch == '.') &&
            (i + 1 < tzItem.size()) &&
            (tzItem[i + 1] == '.')) {
            const size_t next = i + 2;
            const bool isEndOrSlash = (next ==  tzItem.size() || tzItem[next] == '/');
            if (isEndOrSlash) {
                return false;
            }
        }
        lastCh = ch;
    }
    if (tzItem.size() >= PATH_MAX) {
        return false;
    }
    return true;
}

static bool CheckValueSafety(const std::string& value, const std::string& pattern)
{
    if (value.empty() || pattern.empty()) {
        return true;
    }
    
    bool result = MatchesEnvPattern(value, pattern);
    if (result) {
        size_t eqPos = value.find('=');
        if ((eqPos != std::string::npos) && (eqPos + 1 < value.size())) {
            std::string val = value.substr(eqPos + 1);
            if (val.find_first_of("/%") != std::string::npos) {
                return false;
            }
        }
    }
    return true;
}

static bool MatchesEnvCheckPattern(const std::string& value, const std::string& pattern)
{
    bool keepItem = true;
    int tzLength = 3;

    if (strncmp(value.c_str(), "TZ=", tzLength) == 0) {
        keepItem = TzIsSafe(value);
    } else {
        keepItem = CheckValueSafety(value, pattern);
    }
    return keepItem;
}

static bool MatchesEnvCheck(const std::string& envItem)
{
    for (const auto& checkItem : initialCheckenvTables) {
        if (!checkItem.empty()) {
            bool result = MatchesEnvCheckPattern(envItem, checkItem);
            if (!result) {
                return true;
            }
        }
    }
    return false;
}

static bool UpdateEnv()
{
    SaveEnvSnapshot();
    for (const auto& envItem : envSnapshot) {
        size_t sepPos = envItem.find('=');
        std::string envName = (sepPos == std::string::npos) ? envItem : envItem.substr(0, sepPos);

        bool delResult = MatchesEnvDel(envItem);
        if (delResult) {
            if (!envName.empty()) {
                unsetenv(envName.c_str());
                continue;
            }
        }

        bool checkResult = MatchesEnvCheck(envItem);
        if (checkResult) {
            if (!envName.empty()) {
                unsetenv(envName.c_str());
                continue;
            }
        }
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

    if (!UpdateEnv()) {
        WriteStdErr("UpdateEnv failed\n");
        return 1;
    }
    execvp(execCmd, argvNew);
    WriteStdErr("execvp failed\n");
    return 1;
}
