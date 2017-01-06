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

#include "AppUtils.h"

#include <CompilerProxyServer.h>
#include <RemoteToolClient.h>
#include <LocalExecutor.h>

int main(int argc, char** argv)
{
    using namespace Wuild;
    ConfiguredApplication app(argc, argv, "WuildProxyServer", "proxy");

    auto compilerModule = CheckedCreateCompilerModule(app);
    if (!compilerModule)
        return 1;

    CompilerProxyServer::Config proxyConfig;
    if (!app.GetCompilerProxyServerConfig(proxyConfig))
        return 1;

    RemoteToolClient::Config config;
    if (!app.GetRemoteToolClientConfig(config))
        return 1;

    RemoteToolClient rcClient;
    if (!rcClient.SetConfig(config))
        return 1;

    auto localExecutor = LocalExecutor::Create(compilerModule, app.m_tempDir);

    CompilerProxyServer proxyServer(localExecutor, rcClient);
    if (!proxyServer.SetConfig(proxyConfig))
        return 1;

    rcClient.Start();
    proxyServer.Start();

    return ExecAppLoop();
}
