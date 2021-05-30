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

#include "AppUtils.h"

#include <ToolProxyClient.h>
#include <StringUtils.h>

#include <iostream>

int main(int argc, char** argv)
{
    using namespace Wuild;
    ConfiguredApplication app(argc, argv, "WuildProxyClient", "proxy");

    //app.m_loggerConfig.m_maxLogLevel = Syslogger::Notice;
    //app.InitLogging(app.m_loggerConfig);

    ToolProxyClient::Config proxyConfig;
    if (!app.GetToolProxyServerConfig(proxyConfig))
        return 1;

    ToolProxyClient proxyClient;
    if (!proxyClient.SetConfig(proxyConfig))
        return 1;

    if (!proxyClient.Start()) {
        std::cerr << "Failed to connect to WuildProxy!\n";
        return 1;
    }

    proxyClient.RunTask(StringUtils::StringVectorFromArgv(argc, argv));

    return ExecAppLoop();
}
