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
#include "daemon_forward.h"
#include <cstdint>
#include "securec.h"
#include "daemon.h"
#include "jdwp.h"

namespace Hdc {
HdcDaemonForward::HdcDaemonForward(HTaskInfo hTaskInfo)
    : HdcForwardBase(hTaskInfo)
{
}

HdcDaemonForward::~HdcDaemonForward()
{
}

void HdcDaemonForward::SetupJdwpPointCallBack(uv_idle_t *handle)
{
    HCtxForward ctxPoint = (HCtxForward)handle->data;
    HdcDaemonForward *thisClass = reinterpret_cast<HdcDaemonForward *>(ctxPoint->thisClass);
    thisClass->SetupPointContinue(ctxPoint, 1);  // It usually works
    Base::TryCloseHandle((const uv_handle_t *)handle, Base::CloseIdleCallback);
    WRITE_LOG(LOG_DEBUG, "Setup JdwpPointCallBack finish");
    --thisClass->refCount;
    return;
}

bool HdcDaemonForward::SetupJdwpPoint(HCtxForward ctxPoint)
{
    HdcDaemon *daemon = (HdcDaemon *)taskInfo->ownerSessionClass;
    HdcJdwp *clsJdwp = (HdcJdwp *)daemon->clsJdwp;
    uint32_t pid = std::stol(ctxPoint->localArgs[1]);
    if (ctxPoint->checkPoint) {  // checke
        bool ret = clsJdwp->CheckPIDExist(pid);
        SetupPointContinue(ctxPoint, (int)ret);
        WRITE_LOG(LOG_DEBUG, "Jdwp jump checkpoint");
        return true;
    }
    // do slave connect
    // fd[0] for forward, fd[1] for jdwp
    // forward to close fd[0], fd[1] for jdwp close
    int fds[2] = { 0 };
    bool ret = false;
    Base::CreateSocketPair(fds);
    if (uv_tcp_init(loopTask, &ctxPoint->tcp)) {
        return ret;
    }
    ctxPoint->tcp.data = ctxPoint;
    if (uv_tcp_open(&ctxPoint->tcp, fds[0])) {
        return ret;
    }
    constexpr auto len = sizeof(uint32_t);
    uint8_t flag[1 + len + len];
    flag[0] = SP_JDWP_NEWFD;
    if (memcpy_s(flag + 1, sizeof(flag) - 1, &pid, len) ||
        memcpy_s(flag + 1 + len, sizeof(flag) - len - 1, &fds[1], len)) {
        return ret;
    }
    if (ThreadCtrlCommunicate(flag, sizeof(flag)) > 0) {
        ret = true;
    }
    WRITE_LOG(LOG_DEBUG, "SendJdwpNewFD Finish,ret:%d fd0:%d fd1:%d", ret, fds[0], fds[1]);
    if (!ret) {
        Base::CloseSocketPair(fds);
        return ret;
    }

    ++refCount;
    Base::IdleUvTask(loopTask, ctxPoint, SetupJdwpPointCallBack);
    return ret;
}

bool HdcDaemonForward::SetupArkPoint(HCtxForward ctxPoint)
{
    HdcDaemon *daemon = (HdcDaemon *)taskInfo->ownerSessionClass;
    HdcJdwp *clsJdwp = (HdcJdwp *)daemon->clsJdwp;
    std::string ark = ctxPoint->localArgs[0]; // ark
    std::string svr = ctxPoint->localArgs[1]; // pid@tid@Debugger
    std::size_t found = svr.find_first_of("@");
    if (found == std::string::npos) {
        SetupPointContinue(ctxPoint, true);
        WRITE_LOG(LOG_DEBUG, "SetupArkPoint failed id:%s", svr.c_str());
        ctxPoint->lastError = ark + ":" + svr + " parameter invalid";
        return false;
    }
    std::string pidstr = svr.substr(0, found);
    uint32_t pid = std::atoi(pidstr.c_str());
    bool ret = clsJdwp->CheckPIDExist(pid);
    if (!ret) {
        SetupPointContinue(ctxPoint, (int)ret);
        WRITE_LOG(LOG_WARN, "SetupArkPoint failed pid:%u not exist", pid);
        ctxPoint->lastError = ark + ":" + svr + " pid invalid";
        return false;
    }
    // do slave connect
    // fd[0] for forward, fd[1] for ark
    int fds[2] = { 0 };
    ret = false;
    Base::CreateSocketPair(fds);
    if (uv_tcp_init(loopTask, &ctxPoint->tcp)) {
        Base::CloseSocketPair(fds);
        return ret;
    }
    ctxPoint->tcp.data = ctxPoint;
    if (uv_tcp_open(&ctxPoint->tcp, fds[0])) {
        Base::CloseSocketPair(fds);
        return ret;
    }
    std::string str = ark + ":" + svr;
    int size = 1 + sizeof(int32_t) + str.size();
    uint8_t buf[size];
    buf[0] = SP_ARK_NEWFD;
    if (memcpy_s(buf + 1, sizeof(int32_t), &fds[1], sizeof(int32_t)) ||
        memcpy_s(buf + 1 + sizeof(int32_t), str.size(), str.c_str(), str.size())) {
        Base::CloseSocketPair(fds);
        return ret;
    }
    // buf: SP_ARK_NEWFD | fd[1] | pid@tid@Debugger
    if (ThreadCtrlCommunicate(buf, size) > 0) {
        ret = true;
    }
    WRITE_LOG(LOG_DEBUG, "SetupArkPoint Finish,ret:%d fd0:%d fd1:%d", ret, fds[0], fds[1]);
    if (!ret) {
        Base::CloseSocketPair(fds);
        return ret;
    }
    ++refCount;
    Base::IdleUvTask(loopTask, ctxPoint, SetupJdwpPointCallBack);
    return ret;
}
}
