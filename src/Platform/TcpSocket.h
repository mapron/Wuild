/*
 * Copyright (C) 2017-2021 Smirnov Vladimir mapron1@gmail.com
 * Source code licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 or in file COPYING-APACHE-2.0.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.h
 */

#pragma once

#include "TimePoint.h"
#include "IDataSocket.h"
#include "TcpConnectionParams.h"
#include "ThreadLoop.h"

#include <map>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace Wuild {

class TcpListener;
class TcpSocketPrivate;
class HwndWrapper;
/// Implementation of TCP data socket.
/// Can be created by own or through TcpListener::GetPendingConnection.
class TcpSocket : public IDataSocket {
    friend class TcpListener;

public:
    enum class ConnectionState
    {
        None,
        Pending,
        Success,
        Fail
    };
    static std::atomic_bool s_applicationInterruption;

public:
    TcpSocket(const TcpConnectionParams& params);
    virtual ~TcpSocket();

    /// Creates new sockert. If pendingListener is set, then socket will be listener client.
    static IDataSocket::Ptr Create(const TcpConnectionParams& params, TcpListener* pendingListener = nullptr);

    bool Connect() override;
    void Disconnect() override;
    bool IsConnected() const override;
    bool IsPending() const override;

    ReadState  Read(ByteArrayHolder& buffer) override;
    WriteState Write(const ByteArrayHolder& buffer, size_t maxBytes) override;

    /// Socker buffer size available for reading.
    uint32_t GetRecieveBufferSize() const override { return m_recieveBufferSize; }
    uint32_t GetSendBufferSize() const override { return m_sendBufferSize; }

    std::string GetLogContext() const override { return m_logContext; }
    void        SetReadAvailableCallback(const std::function<void()>& cb) override;
    void        SetWaitForRead() override;

protected:
    void SetListener(TcpListener* pendingListener);
    void Fail(); //!< Ошибка при установлении соединения.
    bool IsSocketReadReady();
    bool SelectRead();
    void SetBufferSize();

    TcpListener*        m_pendingListener    = nullptr;
    ConnectionState     m_state              = ConnectionState::None;
    bool                m_acceptedByListener = false;
    TcpConnectionParams m_params;
    uint32_t            m_recieveBufferSize = 0;
    uint32_t            m_sendBufferSize    = 0;
    std::string         m_logContext;
    ThreadLoop          m_socketReadPollThread;

    std::mutex              m_awaitingMutex;
    std::condition_variable m_awaitingCV;
    std::atomic_bool        m_awaitingRead{ true };
    std::function<void()>   m_readPollCallbackInstall{ [] {} };

private:
    std::unique_ptr<TcpSocketPrivate> m_impl;
    std::unique_ptr<HwndWrapper>      m_hwnd;
};

}
