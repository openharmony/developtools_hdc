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

#include "ForwardReadStream_fuzzer.h"
#include <uv.h>
#include "forward.h"

namespace Hdc {
class HdcForwardFuzzer : public HdcForwardBase {
public:
    explicit HdcForwardFuzzer(HTaskInfo hTaskInfo) : HdcForwardBase(hTaskInfo) {}

    static std::unique_ptr<HdcForwardFuzzer> Instance(HTaskInfo hTaskInfo)
    {
        std::unique_ptr<HdcForwardFuzzer> forward = std::make_unique<HdcForwardFuzzer>(hTaskInfo);
        return forward;
    }
};

bool FuzzForwardReadStream(const uint8_t *data, size_t size)
{
    HTaskInfo hTaskInfo = new TaskInformation();
    HdcSessionBase *daemon = new HdcSessionBase(false);
    hTaskInfo->ownerSessionClass = daemon;
    auto forward = HdcForwardFuzzer::Instance(hTaskInfo);
    if (forward == nullptr) {
        WRITE_LOG(LOG_FATAL, "FuzzForwardReadStream forward is null");
        return false;
    }
    HdcForwardBase::HCtxForward ctx = (HdcForwardBase::HCtxForward)forward->MallocContext(true);
    if (ctx == nullptr) {
        WRITE_LOG(LOG_FATAL, "FuzzForwardReadStream ctx is null");
        return false;
    }
    uv_pipe_t pipe;
    pipe.data = ctx;
    uv_stream_t *stream = (uv_stream_t *)&pipe;
    uv_buf_t *rbf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
    if (rbf == nullptr) {
        return false;
    }
    rbf->base = new char[size];
    (void)memcpy_s(rbf->base, size, reinterpret_cast<char *>(const_cast<uint8_t *>(data)), size);
    forward->ReadForwardBuf(stream, (ssize_t)size, rbf);
    delete ctx;
    delete hTaskInfo;
    delete daemon;
    return true;
}
} // namespace Hdc

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    Hdc::FuzzForwardReadStream(data, size);
    return 0;
}
