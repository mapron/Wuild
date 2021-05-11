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

#include <CoordinatorServerConfig.h>

#include <mutex>

namespace Wuild {
class SocketFrameService;
class CoordinatorListResponse;

/// Listens port and sends tool server information to all clients.
class CoordinatorServer {
public:
    using Config = CoordinatorServerConfig;

public:
    CoordinatorServer();
    ~CoordinatorServer();

    bool SetConfig(const Config& config);
    void Start();

protected:
    std::shared_ptr<CoordinatorListResponse> GetResponse();
    Config                                   m_config;
    std::unique_ptr<SocketFrameService>      m_server;
    CoordinatorInfo                          m_info;
    std::mutex                               m_infoMutex;
};

}
