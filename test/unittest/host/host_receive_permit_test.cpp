/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include "host_receive_permit_test.h"

#include "serial_struct.h"
#include "translate.h"

using namespace testing::ext;

namespace Hdc {
namespace {
constexpr uint32_t TEST_CHANNEL_ID = 0x12345678;
constexpr uint32_t TEST_SESSION_ID = 0x10203040;

struct ProxyTcpState {
    HChannel channel = nullptr;
    bool acceptDone = false;
    bool connectDone = false;
    bool listenerClosed = false;
    int acceptStatus = UV_ECANCELED;
    int connectStatus = UV_ECANCELED;
};

void OnProxyListenerClosed(uv_handle_t *handle)
{
    static_cast<ProxyTcpState *>(handle->data)->listenerClosed = true;
}

void OnProxyConnection(uv_stream_t *server, int status)
{
    ProxyTcpState *state = static_cast<ProxyTcpState *>(server->data);
    state->acceptStatus = status == 0 ?
        uv_accept(server, reinterpret_cast<uv_stream_t *>(&state->channel->hWorkTCP)) : status;
    state->acceptDone = true;
    uv_close(reinterpret_cast<uv_handle_t *>(server), OnProxyListenerClosed);
}

void OnProxyConnect(uv_connect_t *request, int status)
{
    ProxyTcpState *state = static_cast<ProxyTcpState *>(request->data);
    state->connectStatus = status;
    state->connectDone = true;
}

void OnProxyTcpClosed(uv_handle_t *handle)
{
    *static_cast<bool *>(handle->data) = true;
}
}

void HdcHostReceivePermitTest::SetUp()
{
    Base::SetLogLevel(LOG_OFF);
    uv_loop_init(&loopMain);
    server = new HdcServer(true);
    serverForClient = new HdcServerForClient(true, "127.0.0.1:0", server, &loopMain);
    server->clsServerForClient = serverForClient;
}

void HdcHostReceivePermitTest::TearDown()
{
    server->clsServerForClient = nullptr;
    Base::TryCloseHandle(reinterpret_cast<uv_handle_t *>(&serverForClient->asyncMainLoop));
    Base::TryCloseHandle(reinterpret_cast<uv_handle_t *>(&serverForClient->loopMainStatus.mReportTimer));
    uv_run(&loopMain, UV_RUN_DEFAULT);
    delete serverForClient;
    delete server;
    EXPECT_EQ(uv_loop_close(&loopMain), 0);
}

HChannel HdcHostReceivePermitTest::CreateChannel(uint32_t channelId, uint32_t sessionId)
{
    HChannel channel = new HdcChannel();
    channel->channelId = channelId;
    channel->targetSessionId = sessionId;
    channel->isDead = false;
    channel->fromClient = false;
    channel->ref = 0;
    return channel;
}

std::string HdcHostReceivePermitTest::SerializeFileConfig(const std::string &cwd, const std::string &path,
    const std::string &optionalName)
{
    HdcTransferBase::TransferConfig config = {};
    config.clientCwd = cwd;
    config.path = path;
    config.optionalName = optionalName;
    return SerialStruct::SerializeToString(config);
}

void HdcHostReceivePermitTest::InitWritableProxy(HChannel channel, uv_tcp_t &peer)
{
    channel->fromClient = true;
    channel->hWorkThread = uv_thread_self();
    channel->clsChannel = serverForClient;
    channel->loopStatus = &serverForClient->loopMainStatus;
    ASSERT_EQ(uv_tcp_init(&loopMain, &channel->hWorkTCP), 0);
    channel->hWorkTCP.data = channel;
    uv_tcp_t listener = {};
    uv_connect_t connectRequest = {};
    ASSERT_EQ(uv_tcp_init(&loopMain, &listener), 0);
    ASSERT_EQ(uv_tcp_init(&loopMain, &peer), 0);
    sockaddr_in address = {};
    ASSERT_EQ(uv_ip4_addr("127.0.0.1", 0, &address), 0);
    ASSERT_EQ(uv_tcp_bind(&listener, reinterpret_cast<const sockaddr *>(&address), 0), 0);
    ProxyTcpState tcpState;
    tcpState.channel = channel;
    listener.data = &tcpState;
    ASSERT_EQ(uv_listen(reinterpret_cast<uv_stream_t *>(&listener), 1, OnProxyConnection), 0);
    int addressLength = sizeof(address);
    ASSERT_EQ(uv_tcp_getsockname(&listener, reinterpret_cast<sockaddr *>(&address), &addressLength), 0);
    connectRequest.data = &tcpState;
    ASSERT_EQ(uv_tcp_connect(&connectRequest, &peer, reinterpret_cast<const sockaddr *>(&address),
        OnProxyConnect), 0);
    while (!tcpState.acceptDone || !tcpState.connectDone || !tcpState.listenerClosed) {
        uv_run(&loopMain, UV_RUN_ONCE);
    }
    ASSERT_EQ(tcpState.acceptStatus, 0);
    ASSERT_EQ(tcpState.connectStatus, 0);
    ASSERT_NE(uv_is_writable(reinterpret_cast<uv_stream_t *>(&channel->hWorkTCP)), 0);
}

void HdcHostReceivePermitTest::CloseWritableProxy(HChannel channel, uv_tcp_t &peer)
{
    serverForClient->AdminChannel(OP_REMOVE, TEST_CHANNEL_ID, nullptr);
    bool channelClosed = false;
    bool peerClosed = false;
    channel->hWorkTCP.data = &channelClosed;
    peer.data = &peerClosed;
    uv_close(reinterpret_cast<uv_handle_t *>(&channel->hWorkTCP), OnProxyTcpClosed);
    uv_close(reinterpret_cast<uv_handle_t *>(&peer), OnProxyTcpClosed);
    while (!channelClosed || !peerClosed) {
        uv_run(&loopMain, UV_RUN_ONCE);
    }
    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, RegisterAbsoluteTargetWithoutCwd, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);

    EXPECT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv /data/local/tmp/source /tmp/host-target"));
    ASSERT_EQ(serverForClient->hostReceivePermits.count(TEST_CHANNEL_ID), 1UL);
    EXPECT_EQ(serverForClient->hostReceivePermits[TEST_CHANNEL_ID].targetPath, "/tmp/host-target");

    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, RegisterRelativeTargetWithoutCwd, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);

    EXPECT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv /data/local/tmp/source relative-target"));
    EXPECT_EQ(serverForClient->hostReceivePermits[TEST_CHANNEL_ID].targetPath, "relative-target");

    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, CheckPermitUsesChannelSessionBinding, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    ASSERT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv -cwd /home/test/ /data/local/tmp/source relative-target"));
    std::string payload = SerializeFileConfig("/home/test/", "relative-target");

    EXPECT_TRUE(serverForClient->CheckHostReceivePermit(channel, TEST_SESSION_ID,
        reinterpret_cast<uint8_t *>(payload.data()), static_cast<int>(payload.size())));
    channel->targetSessionId = TEST_SESSION_ID + 1;
    EXPECT_FALSE(serverForClient->CheckHostReceivePermit(channel, TEST_SESSION_ID,
        reinterpret_cast<uint8_t *>(payload.data()), static_cast<int>(payload.size())));

    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, DuplicateRegistrationClearsPermit, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    ASSERT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv /data/local/tmp/source /tmp/first-target"));

    EXPECT_FALSE(serverForClient->RegisterHostReceivePermit(channel,
        "recv /data/local/tmp/source /tmp/second-target"));
    EXPECT_TRUE(serverForClient->hostReceivePermits.empty());

    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, CheckPermitRejectsEscapingOptionalName, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    ASSERT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv -cwd /home/test/ /data/local/tmp/source target"));
    std::string nestedPayload = SerializeFileConfig("/home/test/", "target", "source/nested.txt");
    std::string dottedNamePayload = SerializeFileConfig("/home/test/", "target", "source/a..b/nested.txt");
    std::string parentTraversalPayload = SerializeFileConfig("/home/test/", "target", "source/../nested.txt");
    std::string escapingPayload = SerializeFileConfig("/home/test/", "target", "source/../../escaped.txt");
    std::string windowsEscapingPayload =
        SerializeFileConfig("/home/test/", "target", "source\\..\\..\\escaped.txt");

    EXPECT_TRUE(serverForClient->CheckHostReceivePermit(channel, TEST_SESSION_ID,
        reinterpret_cast<uint8_t *>(nestedPayload.data()), static_cast<int>(nestedPayload.size())));
    EXPECT_TRUE(serverForClient->CheckHostReceivePermit(channel, TEST_SESSION_ID,
        reinterpret_cast<uint8_t *>(dottedNamePayload.data()), static_cast<int>(dottedNamePayload.size())));
    EXPECT_FALSE(serverForClient->CheckHostReceivePermit(channel, TEST_SESSION_ID,
        reinterpret_cast<uint8_t *>(parentTraversalPayload.data()), static_cast<int>(parentTraversalPayload.size())));
    EXPECT_FALSE(serverForClient->CheckHostReceivePermit(channel, TEST_SESSION_ID,
        reinterpret_cast<uint8_t *>(escapingPayload.data()), static_cast<int>(escapingPayload.size())));
    EXPECT_FALSE(serverForClient->CheckHostReceivePermit(channel, TEST_SESSION_ID,
        reinterpret_cast<uint8_t *>(windowsEscapingPayload.data()), static_cast<int>(windowsEscapingPayload.size())));

    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, ProxyReceiveRegistersPermit, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    channel->connectKey = "test-daemon";
    HdcSession daemonSession;
    daemonSession.sessionId = TEST_SESSION_ID;
    daemonSession.connType = CONN_UNKNOWN;
    server->AdminSession(OP_ADD, TEST_SESSION_ID, &daemonSession);
    HdcDaemonInformation daemonInfo;
    daemonInfo.connectKey = channel->connectKey;
    daemonInfo.connStatus = STATUS_CONNECTED;
    daemonInfo.hSession = &daemonSession;
    HDaemonInfo daemonInfoPtr = &daemonInfo;
    server->AdminDaemonMap(OP_ADD, STRING_EMPTY, daemonInfoPtr);
    TranslateCommand::FormatCommand command = {
        CMD_FILE_INIT, "recv remote -cwd /home/test/ /data/local/tmp/source relative-target", false
    };

    EXPECT_TRUE(serverForClient->TaskCommand(channel, &command));
    EXPECT_TRUE(channel->fromClient);
    auto permit = serverForClient->hostReceivePermits.find(TEST_CHANNEL_ID);
    EXPECT_NE(permit, serverForClient->hostReceivePermits.end());
    if (permit != serverForClient->hostReceivePermits.end()) {
        EXPECT_EQ(permit->second.targetPath, "/home/test/relative-target");
    }

    server->AdminDaemonMap(OP_REMOVE, channel->connectKey, daemonInfoPtr);
    server->AdminSession(OP_REMOVE, TEST_SESSION_ID, nullptr);
    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, DispatcherRejectsMismatchedChannelSession, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    serverForClient->AdminChannel(OP_ADD, TEST_CHANNEL_ID, channel);
    ASSERT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv -cwd /home/test/ /data/local/tmp/source relative-target"));
    HdcSession session;
    session.sessionId = TEST_SESSION_ID + 1;
    session.mapTask = new std::map<uint32_t, HTaskInfo>();
    std::string payload = SerializeFileConfig("/home/test/", "relative-target");

    EXPECT_TRUE(server->FetchCommand(&session, TEST_CHANNEL_ID, CMD_FILE_CHECK,
        reinterpret_cast<uint8_t *>(payload.data()), static_cast<int>(payload.size())));
    EXPECT_TRUE(session.mapTask->empty());
    EXPECT_EQ(channel->ref.load(), 0U);

    serverForClient->AdminChannel(OP_REMOVE, TEST_CHANNEL_ID, nullptr);
    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, DispatcherRejectsDaemonFileInit, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    serverForClient->AdminChannel(OP_ADD, TEST_CHANNEL_ID, channel);
    HdcSession session;
    session.sessionId = TEST_SESSION_ID;
    session.mapTask = new std::map<uint32_t, HTaskInfo>();
    std::string payload = "/home/alice/.ssh/id_rsa dummy";

    EXPECT_TRUE(server->FetchCommand(&session, TEST_CHANNEL_ID, CMD_FILE_INIT,
        reinterpret_cast<uint8_t *>(payload.data()), static_cast<int>(payload.size())));
    EXPECT_TRUE(session.mapTask->empty());
    EXPECT_EQ(channel->ref.load(), 0U);

    serverForClient->AdminChannel(OP_REMOVE, TEST_CHANNEL_ID, nullptr);
    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, DispatcherRejectsDaemonBugreportInit, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    serverForClient->AdminChannel(OP_ADD, TEST_CHANNEL_ID, channel);
    HdcSession session;
    session.sessionId = TEST_SESSION_ID;
    session.mapTask = new std::map<uint32_t, HTaskInfo>();
    std::string payload = "/tmp/daemon-selected-report";

    EXPECT_TRUE(server->FetchCommand(&session, TEST_CHANNEL_ID, CMD_UNITY_BUGREPORT_INIT,
        reinterpret_cast<uint8_t *>(payload.data()), static_cast<int>(payload.size())));
    EXPECT_TRUE(session.mapTask->empty());
    EXPECT_EQ(channel->ref.load(), 0U);

    serverForClient->AdminChannel(OP_REMOVE, TEST_CHANNEL_ID, nullptr);
    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, DispatcherRejectsDaemonBugreportInitWithFilePermit, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    serverForClient->AdminChannel(OP_ADD, TEST_CHANNEL_ID, channel);
    ASSERT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv /data/local/tmp/source /tmp/host-target"));
    HdcSession session;
    session.sessionId = TEST_SESSION_ID;
    session.mapTask = new std::map<uint32_t, HTaskInfo>();
    std::string payload = "/tmp/host-target";

    EXPECT_TRUE(server->FetchCommand(&session, TEST_CHANNEL_ID, CMD_UNITY_BUGREPORT_INIT,
        reinterpret_cast<uint8_t *>(payload.data()), static_cast<int>(payload.size())));
    EXPECT_TRUE(session.mapTask->empty());
    EXPECT_EQ(channel->ref.load(), 0U);

    serverForClient->AdminChannel(OP_REMOVE, TEST_CHANNEL_ID, nullptr);
    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, ProxyFileCheckRequiresHostPermit, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    uv_tcp_t peer = {};
    ASSERT_NO_FATAL_FAILURE(InitWritableProxy(channel, peer));

    serverForClient->AdminChannel(OP_ADD, TEST_CHANNEL_ID, channel);
    ASSERT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv -cwd /home/test/ /data/local/tmp/source safe"));
    HdcSession session;
    session.sessionId = TEST_SESSION_ID;
    session.mapTask = new std::map<uint32_t, HTaskInfo>();
    std::string checkPayload = SerializeFileConfig("", "/home/alice/.ssh/authorized_keys");

    EXPECT_TRUE(server->FetchCommand(&session, TEST_CHANNEL_ID, CMD_FILE_CHECK,
        reinterpret_cast<uint8_t *>(checkPayload.data()), static_cast<int>(checkPayload.size())));
    EXPECT_TRUE(session.mapTask->empty());
    EXPECT_EQ(channel->ref.load(), 0U);

    std::string traversalPayload = SerializeFileConfig("/home/test/", "safe", "source/../escaped");
    EXPECT_TRUE(server->FetchCommand(&session, TEST_CHANNEL_ID, CMD_FILE_CHECK,
        reinterpret_cast<uint8_t *>(traversalPayload.data()), static_cast<int>(traversalPayload.size())));
    EXPECT_EQ(channel->ref.load(), 0U);

    std::string validPayload = SerializeFileConfig("/home/test/", "safe");
    EXPECT_TRUE(server->FetchCommand(&session, TEST_CHANNEL_ID, CMD_FILE_CHECK,
        reinterpret_cast<uint8_t *>(validPayload.data()), static_cast<int>(validPayload.size())));
    EXPECT_EQ(channel->ref.load(), 1U);
    while (channel->ref.load() != 0) {
        uv_run(&loopMain, UV_RUN_ONCE);
    }

    CloseWritableProxy(channel, peer);
}

HWTEST_F(HdcHostReceivePermitTest, RegisterReceiveWithSupportedOptions, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);

    EXPECT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv -cwd /home/test/ -z -sync -a -m -b com.example.app source relative-target"));
    ASSERT_EQ(serverForClient->hostReceivePermits.count(TEST_CHANNEL_ID), 1UL);
    EXPECT_EQ(serverForClient->hostReceivePermits[TEST_CHANNEL_ID].targetPath, "/home/test/relative-target");

    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, RegisterReceiveWithDefaultTarget, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);

    ASSERT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv -cwd /home/test/ remote-source"));
    EXPECT_EQ(serverForClient->hostReceivePermits[TEST_CHANNEL_ID].targetPath, "/home/test/.");
    std::string payload = SerializeFileConfig("/home/test/", ".");
    EXPECT_TRUE(serverForClient->CheckHostReceivePermit(channel, TEST_SESSION_ID,
        reinterpret_cast<uint8_t *>(payload.data()), static_cast<int>(payload.size())));

    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, FailedRegistrationClearsStalePermit, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    ASSERT_TRUE(serverForClient->RegisterHostReceivePermit(channel,
        "recv /data/local/tmp/source /tmp/host-target"));

    EXPECT_FALSE(serverForClient->RegisterHostReceivePermit(channel, "recv -cwd /home/test/ -b"));
    EXPECT_TRUE(serverForClient->hostReceivePermits.empty());

    delete channel;
}

HWTEST_F(HdcHostReceivePermitTest, PermitFailureDoesNotRejectTaskCommand, TestSize.Level0)
{
    HChannel channel = CreateChannel(TEST_CHANNEL_ID, TEST_SESSION_ID);
    channel->hWorkThread = uv_thread_self();
    ASSERT_EQ(uv_tcp_init(&loopMain, &channel->hWorkTCP), 0);
    TranslateCommand::FormatCommand command = {
        CMD_FILE_INIT, "recv -cwd /home/test/ -b", false
    };

    EXPECT_TRUE(serverForClient->TaskCommand(channel, &command));
    EXPECT_TRUE(serverForClient->hostReceivePermits.empty());

    uv_close(reinterpret_cast<uv_handle_t *>(&channel->hWorkTCP), nullptr);
    uv_run(&loopMain, UV_RUN_NOWAIT);
    delete channel;
}
} // namespace Hdc
