/*
 * Copyright (C) 2017 Smirnov Vladimir mapron1@gmail.com
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

#include "CoordinatorTypes.h"

#include <ThreadLoop.h>
#include <CoordinatorClientConfig.h>

#include <functional>
#include <atomic>
#include <mutex>

namespace Wuild {
class SocketFrameHandler;

/// Retrives and send tool server information.
class CoordinatorClient {
public:
    using InfoArrivedCallback = std::function<void(const CoordinatorInfo&)>;
    using Config              = CoordinatorClientConfig;

public:
    CoordinatorClient();
    ~CoordinatorClient();

    bool SetConfig(const Config& config);
    void SetInfoArrivedCallback(InfoArrivedCallback callback);

    void Start();

    void SetToolServerInfo(const ToolServerInfo& info);
    void SendToolServerSessionInfo(const ToolServerSessionInfo& sessionInfo, bool isFinished);

    void StopExtraClients(const std::string& hostExcept);

protected:
    InfoArrivedCallback m_infoArrivedCallback;

    struct CoordWorker {
        CoordinatorClient*                  m_coordClient = nullptr;
        std::shared_ptr<SocketFrameHandler> m_client;
        std::atomic_bool                    m_clientState{ false };

        std::atomic_bool m_needSendToolServerInfo{ true };
        std::atomic_bool m_needRequestData{ true };
        TimePoint        m_lastSend;

        ThreadLoop  m_thread;
        std::string m_host;

        ToolServerInfo m_toolServerInfo;
        std::mutex     m_toolServerInfoMutex;

        void SetToolServerInfo(const ToolServerInfo& info);

        void Quant();
        void Start(const std::string& host, int port);
        void Stop();
        ~CoordWorker();
    };

    std::deque<std::shared_ptr<CoordWorker>> m_workers;

    CoordinatorInfo m_coord;
    ToolServerInfo  m_lastInfo;

    std::mutex m_coordMutex;

    std::atomic_bool m_exclusiveModeSet{ false };

    Config m_config;
};

}
