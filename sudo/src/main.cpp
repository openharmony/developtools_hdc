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
#include <climits>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
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
static const std::vector<std::string> DEFAULT_PATH = {"/bin", "/usr/bin", "/system/bin", "/vendor/bin",
    "/usr/local/bin", "/data/app/bin", "/data/service/hnp/bin", "/opt/homebrew"};
static const char *DEFAULT_BASH = "/system/bin/sh";
static FILE *g_ttyFp = nullptr;
static const char *OUT_OF_MEM = "[E0001] out of memory\n";
static const char *COMMAND_NOT_FOUND = "[E0002] command not found\n";
static const char *USER_VERIFY_FAILED = "[E0003] Sorry, try again. If screen lock password not set, set it first.\n";
static const char *USER_SWITCH_FAILED = "[E0004] Currently logged-in user has switched, please try again.\n";
static const char *HELP = "sudo - execute command or script as root\n\n"
                       "usage: sudo command ...\n";
static int32_t g_userId = -1;
static std::vector<uint8_t> g_challenge(CHALLENGE_LEN, 0);
static std::vector<uint8_t> g_authToken = {0};
static const std::string CONSTRAINT_SUDO = "constraint.sudo";
static const std::string TITLE = "Allow execution of sudo commands";

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

enum class CmdType : int32_t {
    INVALID = -1,
    SCRIPT = 0,
    BINARY = 1
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

/**
 * elf magic number: 0x7f, 'E', 'L', 'F'
 */
static bool IsElf(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    const int magicCount = 4;
    const char magicNum[magicCount] = {0x7f, 'E', 'L', 'F'};
    unsigned char magic[magicCount];
    file.read(reinterpret_cast<char*>(magic), magicCount);
    if (file.gcount() < magicCount) {
        return false;
    }

    for (int i = 0; i < magicCount; i++) {
        if (magic[i] != magicNum[i]) {
            return false;
        }
    }
    return true;
}

/*
 * Find cmd from DEFAULT_PATH
 * return:
 * -1 (not exists | not support exec)
 * 0 (.sh file)
 * 1 (elf file)
*/
static CmdType GetCmdInPath(char *cmd, int cmdBufLen)
{
    std::string cmdStr(cmd);
    struct stat st;

    if (cmdStr.find('/') != std::string::npos) {
        if (stat(cmdStr.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            if (IsElf(cmdStr)) {
                return CmdType::BINARY;
            }
            return CmdType::SCRIPT;
        } else {
            WriteTty(COMMAND_NOT_FOUND);
            return CmdType::INVALID;
        }
    }

    for (const auto &path : DEFAULT_PATH) {
        std::string cmdPath = path.empty() ? cmd : path + "/" + cmd;
        if (stat(cmdPath.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            size_t len = cmdPath.length();
            int ret = memcpy_s(cmd, cmdBufLen - 1, cmdPath.c_str(), len);
            if (ret != EOK) {
                WriteTty(COMMAND_NOT_FOUND);
                return CmdType::INVALID;
            }
            return CmdType::BINARY;
        }
    }

    WriteTty(COMMAND_NOT_FOUND);
    return CmdType::INVALID;
}

static char** NewCStringArray(int size)
{
    if (size <= 0 || size > SUDO_ARG_MAX_NUMS) {
        return nullptr;
    }
    char** array = new(std::nothrow) char* [size];
    if (array == nullptr) {
        WriteStdErr(OUT_OF_MEM);
        return nullptr;
    }
    (void)memset_s(array, sizeof(char*) * size, 0, sizeof(char*) * size);
    return array;
}

static char** ParseShCCmd(int argc, char* argv[])
{
    int argvNewIndex = 0;
    int startCopyArgvIndex = 3;
    // sudo {"sh", "-c", "xxxx", nullptr}
    char** argvTmp = NewCStringArray(argc + 1);
    if (argvTmp == nullptr) {
        return nullptr;
    }

    argvTmp[argvNewIndex++] = StrDup(DEFAULT_BASH);
    argvTmp[argvNewIndex++] = StrDup(argv[startCopyArgvIndex - 1]);

    for (int i = startCopyArgvIndex; i < argc; i++) {
        argvTmp[argvNewIndex++] = StrDup(argv[i]);
    }
    argvTmp[argvNewIndex] = nullptr;

    return argvTmp;
}

static char** ParseBinaryCmd(int argc, char* argv[], const char* cmd)
{
    int argvNewIndex = 0;
    int startCopyArgvIndex = 2;
    // sudo {"execSh", "xxxx", nullptr}
    char** argvTmp = NewCStringArray(argc + 1);
    if (argvTmp == nullptr) {
        return nullptr;
    }

    argvTmp[argvNewIndex++] = StrDup(cmd);
    
    for (int i = startCopyArgvIndex; i < argc; i++) {
        argvTmp[argvNewIndex++] = StrDup(argv[i]);
    }
    argvTmp[argvNewIndex] = nullptr;

    return argvTmp;
}

static char** ParseScriptCmd(int argc, char* argv[])
{
    int argvNewIndex = 0;
    int startCopyArgvIndex = 1;
    const int argcAdd = 3;
    const char* shStr = "sh";
    const char* cStr = "-c";
    // sudo {"xxxx", nullptr} => sudo {"sh", "-c", "xxxx", nullptr}
    char** argvTmp = NewCStringArray(argc + argcAdd);
    if (argvTmp == nullptr) {
        return nullptr;
    }

    argvTmp[argvNewIndex++] = StrDup(shStr);
    argvTmp[argvNewIndex++] = StrDup(cStr);

    for (int i = startCopyArgvIndex; i < argc; i++) {
        argvTmp[argvNewIndex++] = StrDup(argv[i]);
    }
    argvTmp[argvNewIndex] = nullptr;

    return argvTmp;
}

static char **ParseCmd(int argc, char* argv[])
{
    bool isShc = false;
    const int argcCountLimit = 3;
    const int idxTwo = 2;

    if ((argc <= 0) || (argc > SUDO_ARG_MAX_NUMS)) {
        return nullptr;
    }

    // sudo sh -c xxxx
    if (argc >= argcCountLimit && strcmp(argv[1], "sh") == 0 && strcmp(argv[idxTwo], "-c") == 0) { //3:argc of main
        isShc = true;
        return ParseShCCmd(argc, argv);
    }

    /*
     * if not "sudo sh -c xxxx", just as "sudo xxxx"
     * 1. check xxxx by DEFAULT_PATH firstly.
     * 2. if not 1, exec shell file.
    */
    if (!isShc) {
        char cmd[PATH_MAX + 1] = {0};
        int ret = sprintf_s(cmd, PATH_MAX + 1, "%s", argv[1]);
        if (ret < 0) {
            return nullptr;
        }
        auto type = GetCmdInPath(cmd, PATH_MAX + 1);
        switch (type) {
            case CmdType::BINARY:
                return ParseBinaryCmd(argc, argv, cmd);
            case CmdType::SCRIPT: {
                return ParseScriptCmd(argc, argv);
            }
            default:
                return nullptr;
        }
    }

    return nullptr;
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

static int32_t GetChallenge()
{
    if (g_userId == -1) {
        WriteStdErr("GetChallenge userid is failed!\n");
        return -1;
    }
    int32_t status = -1;
    int32_t res = InitChallengeForCommandExt(g_userId, g_challenge.data(), g_challenge.size(), &status);
    if (res != 0) {
        switch (res) {
            case AclMgrResultCode::SEC_USERID_CONSTRAINED_ERROR:
                WriteStdErr("no permission.\n");
                break;
            case AclMgrResultCode::SEC_PERMISSION_DENIED:
                WriteStdErr("current user is not an administrator, please try again using an administrator account.\n");
                break;
            default:
                WriteStdErr("init challenge failed\n");
        }
    }
    return status;
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

static bool SetPSL(bool expired = false)
{
    uint32_t len = expired ? 0 : g_authToken.size();
    int32_t res = SetProcessLevelByCommand(g_authToken.data(), len);
    return res == 0;
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

static bool Verify()
{
    int retryTimes = 3;
    while (retryTimes-- > 0) {
        int32_t res = GetChallenge();
        if (res < 0) {
            return false;
        }
        // have cache
        if (res == 1) {
            // check cache expired
            if (SetPSL(true)) {
                // cache is not expired
                return true;
            }
            // cache is expired, verify again
            continue;
        }

        if (!VerifyUserPin() || !SetPSL()) {
            return false;
        }
        return true;
    }

    return false;
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

    if (!Verify()) {
        return 1;
    }

    char **argvNew = ParseCmd(argc, argv);
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
    execvp(argvNew[0], argvNew);
    WriteStdErr("execvp failed\n");
    return 1;
}
