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

#include "TestUtils.h"

#include <ArgStorage.h>
#include <FileUtils.h>
#include <LocalExecutor.h>
#include <RemoteToolClient.h>
#include <VersionChecker.h>

/*
 * Test Wuild compiler and coordinator configuration. Acts just as first configured toolchain.
 * Run this test as compiler invocation, for exemple
 * TestAllConfigs -c test.cpp -o test.o
 */
int main(int argc, char** argv)
{
    using namespace Wuild;
    ArgStorage            argStorage(argc, argv);
    ConfiguredApplication app(argStorage.GetConfigValues(), "TestAllConfigs");
    if (!CreateInvocationRewriter(app))
        return 1;

    Syslogger() << "Configuration: " << app.DumpAllConfigValues();

    //app.m_loggerConfig.m_maxLogLevel = Syslogger::Debug;
    app.InitLogging(app.m_loggerConfig);

    const auto args = argStorage.GetArgs();

    auto localExecutor = LocalExecutor::Create(TestConfiguration::s_invocationRewriter, app.m_tempDir);

    auto        versionChecker = VersionChecker::Create(localExecutor, TestConfiguration::s_invocationRewriter);
    const auto& toolsConfig    = TestConfiguration::s_invocationRewriter->GetConfig();
    const auto  toolsVersions  = versionChecker->DetermineToolVersions({});
    for (const auto& toolId : toolsConfig.m_toolIds)
        Syslogger(Syslogger::Notice) << "tool[" << toolId << "] version=" << toolsVersions.at(toolId);

    std::string            err;
    LocalExecutorTask::Ptr original(new LocalExecutorTask());
    original->m_readOutput = original->m_writeInput = false;
    original->m_invocation                          = ToolInvocation(args).SetExecutable(toolsConfig.GetFirstToolName());
    auto tasks                                      = localExecutor->SplitTask(original, err);
    if (!tasks.first) {
        Syslogger(Syslogger::Err) << err;
        return 1;
    }
    LocalExecutorTask::Ptr taskPP = tasks.first;
    LocalExecutorTask::Ptr taskCC = tasks.second;

    RemoteToolClient::Config config;
    if (!app.GetRemoteToolClientConfig(config))
        return 1;

    RemoteToolClient rcClient(TestConfiguration::s_invocationRewriter, toolsVersions);
    config.m_queueTimeout = TimePoint(3.0);

    if (!rcClient.SetConfig(config))
        return 1;

    rcClient.Start();

    taskPP->m_callback = [&rcClient, taskCC](LocalExecutorResult::Ptr localResult) {
        if (!localResult->m_result) {
            Syslogger(Syslogger::Err) << "Preprocess failed";
            Application::Interrupt(1);
            return;
        }

        auto callback = [](const Wuild::RemoteToolClient::TaskExecutionInfo& info) {
            if (!info.m_stdOutput.empty())
                std::cout << info.m_stdOutput << std::endl
                          << std::flush;
            std::cout << info.GetProfilingStr() << " \n";
            Application::Interrupt(1 - info.m_result);
        };
        rcClient.InvokeTool(taskCC->m_invocation, callback);
    };
    localExecutor->AddTask(taskPP);

    return ExecAppLoop(TestConfiguration::ExitHandler);
}
