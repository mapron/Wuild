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

#include "TestUtils.h"

#include <ArgStorage.h>
#include <RemoteToolClient.h>
#include <RemoteToolServer.h>
#include <LocalExecutor.h>
#include <VersionChecker.h>

const int                    g_toolsServerTestPort = 12345;
const Wuild::CompressionType g_compType            = Wuild::CompressionType::ZStd;

/*
 * This test should be invoked just as normal compiler; first toolId used.
 * Test checks compiler configuration + checks network layer, but does not use coordinator or toolserver config.
 */
int main(int argc, char** argv)
{
    using namespace Wuild;
    ArgStorage            argStorage(argc, argv);
    ConfiguredApplication app(argStorage.GetConfigValues(), "TestToolServer");
    if (!CreateInvocationToolProvider(app))
        return 1;

    StringVector args          = argStorage.GetArgs();
    auto         localExecutor = LocalExecutor::Create(TestConfiguration::s_invocationToolProvider, app.m_tempDir);

    std::string            err;
    LocalExecutorTask::Ptr original(new LocalExecutorTask());
    original->m_readOutput = original->m_writeInput = false;
    original->m_invocation                          = ToolCommandline(args).SetExecutable(TestConfiguration::s_invocationConfig.GetFirstToolName());
    auto tasks                                      = localExecutor->SplitTask(original, err);
    if (!tasks.first) {
        Syslogger(Syslogger::Err) << err;
        return 1;
    }
    LocalExecutorTask::Ptr taskPP = tasks.first;
    LocalExecutorTask::Ptr taskCC = tasks.second;

    RemoteToolServer::Config toolServerConfig;
    toolServerConfig.m_listenHost            = "localhost";
    toolServerConfig.m_listenPort            = g_toolsServerTestPort;
    toolServerConfig.m_coordinator.m_enabled = false;
    toolServerConfig.m_compression.m_type    = g_compType;

    const auto toolsVersions = VersionChecker::Create(localExecutor, TestConfiguration::s_invocationToolProvider)->DetermineToolVersions({});

    RemoteToolServer rcServer(localExecutor, toolsVersions);
    if (!rcServer.SetConfig(toolServerConfig))
        return 1;

    rcServer.Start();

    RemoteToolClient rcClient(TestConfiguration::s_invocationToolProvider, toolsVersions);
    ToolServerInfo   toolServerInfo;
    toolServerInfo.m_connectionHost = "localhost";
    toolServerInfo.m_connectionPort = g_toolsServerTestPort;
    toolServerInfo.m_toolIds        = TestConfiguration::s_invocationConfig.m_toolIds;
    toolServerInfo.m_totalThreads   = 1;
    rcClient.AddClient(toolServerInfo);
    RemoteToolClient::Config clientConfig;
    clientConfig.m_coordinator.m_enabled = false;
    clientConfig.m_compression.m_type    = g_compType;
    rcClient.SetConfig(clientConfig);
    rcClient.Start();

    rcClient.SetRemoteAvailableCallback([taskPP, &rcClient, taskCC, &localExecutor]() {
        taskPP->m_callback = [&rcClient, taskCC](LocalExecutorResult::Ptr localResult) {
            if (!localResult->m_result) {
                Syslogger(Syslogger::Err) << "Preprocess failed\n"
                                          << localResult->m_stdOut;
                Application::Interrupt(1);
                return;
            }

            auto callback = [&rcClient](const Wuild::RemoteToolClient::TaskExecutionInfo& info) {
                if (!info.m_stdOutput.empty())
                    std::cout << info.m_stdOutput << std::endl
                              << std::flush;
                rcClient.FinishSession();
                std::cout << rcClient.GetSessionInformation() << " \n";
                Application::Interrupt(1 - info.m_result);
            };
            rcClient.InvokeTool(taskCC->m_invocation, callback);
        };
        localExecutor->AddTask(taskPP);
    });

    return ExecAppLoop(TestConfiguration::ExitHandler);
}
