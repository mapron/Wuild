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

#include "CompilerProxyFrames.h"

#include <CompilerProxyServerConfig.h>
#include <RemoteToolClient.h>
#include <ILocalExecutor.h>

#include <mutex>

namespace Wuild
{
class SocketFrameService;
/// TODO:
class CompilerProxyServer
{
public:
    using Config = CompilerProxyServerConfig;

public:
    CompilerProxyServer(ILocalExecutor::Ptr executor, RemoteToolClient & rcClient);
    ~CompilerProxyServer();

    bool SetConfig(const Config & config);
    void Start();

protected:
    ILocalExecutor::Ptr m_executor;
    RemoteToolClient & m_rcClient;
    Config m_config;
    std::unique_ptr<SocketFrameService> m_server;
};

}
