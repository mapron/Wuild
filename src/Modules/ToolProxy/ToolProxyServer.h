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

#include "ToolProxyFrames.h"

#include <ToolProxyServerConfig.h>
#include <RemoteToolClient.h>
#include <ILocalExecutor.h>
#include <ThreadLoop.h>

#include <mutex>

namespace Wuild {
class SocketFrameService;
/**
 * @brief Proxy for remote command execution
 *
 * -listen for local invocations;
 * -communicate with coordinator;
 * -split commands;
 * -send requests to remote servers;
 * -when request is done, result is sent to local proxy client.
 */
class ToolProxyServer {
public:
    using Config = ToolProxyServerConfig;

public:
    ToolProxyServer(ILocalExecutor::Ptr executor, RemoteToolClient& rcClient);
    ~ToolProxyServer();

    bool SetConfig(const Config& config);
    void Start(std::function<void()> interruptCallback);

private:
    void UpdateRunningJobs(int delta);

private:
    ILocalExecutor::Ptr                 m_executor;
    RemoteToolClient&                   m_rcClient;
    Config                              m_config;
    std::string                         m_cwd;
    std::unique_ptr<SocketFrameService> m_server;
    ThreadLoop                          m_inactiveChecker;
    int                                 m_runningJobs = 0;
    TimePoint                           m_runningJobsUpdate;
    std::mutex                          m_runningMutex;
};

}
