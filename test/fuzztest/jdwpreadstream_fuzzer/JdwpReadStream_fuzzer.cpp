/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "JdwpReadStream_fuzzer.h"
#include <uv.h>

namespace Hdc {
class HdcJdwpFuzzer : public HdcJdwp {
public:
    HdcJdwpFuzzer(uv_loop_t *loop) : HdcJdwp(loop) {}

    static std::unique_ptr<HdcJdwpFuzzer> Instance(uv_loop_t *loop)
    {
        std::unique_ptr<HdcJdwpFuzzer> jdwp = std::make_unique<HdcJdwpFuzzer>(loop);
        if (jdwp == nullptr) {
            WRITE_LOG(LOG_FATAL, "Error in HdcJdwpFuzzer::instance make_unique failed");
            return nullptr;
        }
        return jdwp;
    }
};

bool FuzzJdwpReadStream(const uint8_t *data, size_t size)
{
    uv_loop_t loop;
    uv_loop_init(&loop);
    auto jdwp = HdcJdwpFuzzer::Instance(&loop);
    if (jdwp == nullptr) {
        WRITE_LOG(LOG_FATAL, "FuzzJdwpReadStream jdwp is null");
        return false;
    }
    HdcJdwp::HCtxJdwp ctx = (HdcJdwp::HCtxJdwp)jdwp->MallocContext();
    if (ctx == nullptr) {
        WRITE_LOG(LOG_FATAL, "FuzzJdwpReadStream jdwp MallocContext failed");
        return false;
    }
    ctx->finish = true;
    uv_pipe_t pipe;
    pipe.data = ctx;
    uv_stream_t *stream = (uv_stream_t *)&pipe;
    uv_buf_t rbf = uv_buf_init((char *)data, size);
    jdwp->ReadStream(stream, (ssize_t)size, &rbf);
    delete ctx;
    uv_stop(&loop);
    uv_loop_close(&loop);
    return true;
}
} // namespace Hdc

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    Hdc::FuzzJdwpReadStream(data, size);
    return 0;
}
