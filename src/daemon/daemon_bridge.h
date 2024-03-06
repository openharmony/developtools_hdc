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
#ifndef HDC_DAEMON_BRIDGE_H
#define HDC_DAEMON_BRIDGE_H
#include <cstdint>
#include "uv.h"
#include "uv/unix.h"
#include "tcp.h"
#include <linux/ioctl.h>
#include <sys/ioctl.h>

#define IOC_MAGIC 0xE6

#define IOC_BIND _IOW(IOC_MAGIC, 1, int)
#define IOC_CONNECT _IOW(IOC_MAGIC, 2, int)

#define SOCKET_FD_LEN 4

namespace Hdc {
class HdcDaemonBridge : public HdcTCPBase {
public:
    HdcDaemonBridge(const bool serverOrDaemonIn, void *ptrMainBase);
    ~HdcDaemonBridge() override;
    uint16_t bridgeListenPort;
    int Initial();
    void Stop();

private:
    static void AllocBufferCallback(uv_handle_t *handle, size_t sizeSuggested, uv_buf_t *buf);
    static void AcceptClient(uv_stream_t *uvpipe, ssize_t nread, const uv_buf_t *buf);
    int SetBridgeListen();
    uv_pipe_t servPipe = {};
};
}  // namespace Hdc

#endif
