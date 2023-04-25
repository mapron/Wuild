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

#include "SocketFrame.h"
#include "SocketFrameHandler.h"
#include "IDataListener.h"
#include "ThreadLoop.h"

#include <mutex>
#include <list>
#include <functional>
#include <memory>

namespace Wuild {

/**
 * \brief Channel-level message service. Listens TCP-port and creates SocketFrameHandler for each connection.
 *
 * On connection lost, SocketFrameHandler will be deleted.
 * To send message to all clients, use QueueFrameToAll().
 * To register frame handlers, use either RegisterFrameReader or SetHandlerInitCallback
 */
class SocketFrameService final {
public:
    using HandlerInitCallback    = std::function<void(SocketFrameHandler*)>;
    using HandlerDestroyCallback = std::function<void(SocketFrameHandler*)>;

public:
    SocketFrameService(const SocketFrameHandlerSettings& settings = SocketFrameHandlerSettings(), int autoStartListenPort = -1, const StringVector& whiteList = StringVector());
    virtual ~SocketFrameService();

    /// Sends message to all connected clients.
    int QueueFrameToAll(SocketFrameHandler* sender, const SocketFrame::Ptr& message);

    /// Creates new tcp port listener.
    void AddTcpListener(int port, const std::string& host = "*", const StringVector& whiteList = StringVector(), std::function<void()> connectionFailureCallback = {});

    /// Starts new processing thread, calling Quant().
    void Start();
    /// Stops service
    void Stop();
    /// Main channel logic: connecting new clients and removing disconnected.
    bool Quant();

    /// At least one active client connected.
    bool IsConnected();

    size_t GetActiveConnectionsCount();

    /// Register frame reader for all clients. @see SocketFrameHandler::RegisterFrameReader
    void RegisterFrameReader(const SocketFrameHandler::IFrameReader::Ptr& reader);

    /// Sets callback for new client connection
    void SetHandlerInitCallback(HandlerInitCallback callback);

    /// Sets callback for client disconnection. Note: queueing new frames will no effect.
    void SetHandlerDestroyCallback(HandlerDestroyCallback callback);

protected:
    void AddWorker(IDataSocket::Ptr client, int threadId);

protected:
    const SocketFrameHandlerSettings m_settings;
    using DeadClient = std::pair<SocketFrameHandler::Ptr, TimePoint>;

    std::deque<SocketFrameHandler::IFrameReader::Ptr> m_readers;
    std::deque<IDataListener::Ptr>                    m_listenters;
    std::deque<SocketFrameHandler::Ptr>               m_workers;
    std::mutex                                        m_workersLock;

    HandlerInitCallback    m_handlerInitCallback;
    HandlerDestroyCallback m_handlerDestroyCallback;

    uint32_t    m_nextWorkerId{ 0 };
    ThreadLoop  m_mainThread;
    std::string m_logContext;
    TimePoint   m_lastLog;
};

}
