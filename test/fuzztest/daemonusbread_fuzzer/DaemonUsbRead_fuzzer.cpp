/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "DaemonUsbRead_fuzzer.h"
#include "daemon_usb.h"
#include <uv.h>

namespace Hdc {
class HdcDaemonUsbFuzzer : public HdcDaemonUSB {
public:
    explicit HdcDaemonUsbFuzzer(void *ptrbase) : HdcDaemonUSB(false, ptrbase) {}

    static std::unique_ptr<HdcDaemonUsbFuzzer> Instance(void *ptrbase)
    {
        std::unique_ptr<HdcDaemonUsbFuzzer> daemonusb = std::make_unique<HdcDaemonUsbFuzzer>(ptrbase);
        return daemonusb;
    }
};

bool FuzzDaemonUsbRead(const uint8_t *data, size_t size)
{
    auto daemonusb = HdcDaemonUsbFuzzer::Instance(nullptr);
    if (daemonusb == nullptr) {
        WRITE_LOG(LOG_FATAL, "FuzzDaemonUsbRead daemonusb is null");
        return false;
    }
    HdcUSB hUSB = {};
    hUSB.wMaxPacketSizeSend = MAX_PACKET_SIZE_HISPEED;
    HdcDaemonUSB::CtxUvFileCommonIo ctxRecv = {};
    ctxRecv.thisClass = &daemonusb;
    ctxRecv.data = &hUSB;
    ctxRecv.bufSizeMax = Base::GetUsbffsBulkSize();
    ctxRecv.buf = const_cast<uint8_t *>(data);
    ctxRecv.req = {};
    uv_fs_t *req = &ctxRecv.req;
    req->result = size;
    req->data = &ctxRecv;
    daemonusb->OnUSBRead(req);
    return true;
}
} // namespace Hdc

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Hdc::FuzzDaemonUsbRead(data, size);
    return 0;
}
