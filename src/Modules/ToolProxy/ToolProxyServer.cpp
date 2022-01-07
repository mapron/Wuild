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

#include "ToolProxyServer.h"

#include <SocketFrameService.h>
#include <FileUtils.h>

#include <utility>
#include <memory>

namespace Wuild {

ToolProxyServer::ToolProxyServer(ILocalExecutor::Ptr executor, RemoteToolClient& rcClient)
    : m_executor(std::move(std::move(executor)))
    , m_rcClient(rcClient)
{
}

ToolProxyServer::~ToolProxyServer()
{
    m_server.reset();
}

bool ToolProxyServer::SetConfig(const ToolProxyServer::Config& config)
{
    std::ostringstream os;
    if (!config.Validate(&os)) {
        Syslogger(Syslogger::Err) << os.str();
        return false;
    }
    m_config = config;
    return true;
}

void ToolProxyServer::Start(std::function<void()> interruptCallback)
{
    m_executor->SetThreadCount(m_config.m_threadCount);
    m_server = std::make_unique<SocketFrameService>();
    m_server->AddTcpListener(m_config.m_listenPort, "*", {}, interruptCallback);

    m_server->RegisterFrameReader(SocketFrameReaderTemplate<ToolProxyRequest>::Create([this](const ToolProxyRequest& inputMessage, SocketFrameHandler::OutputCallback outputCallback) {
        // TODO: we assume that proxy server is used to build only one working directory at once.
        if (m_cwd != inputMessage.m_cwd) {
            m_cwd = inputMessage.m_cwd;
            SetCWD(m_cwd);
        }

        LocalExecutorTask::Ptr original(new LocalExecutorTask());
        original->m_invocation = inputMessage.m_invocation;

        original->m_readOutput = original->m_writeInput = false;
        std::string              err;
        ILocalExecutor::TaskPair tasks = m_executor->SplitTask(original, err);
        UpdateRunningJobs(+1);
        if (tasks.first) {
            LocalExecutorTask::Ptr taskPP = tasks.first;

            taskPP->m_callback = [this, taskCC = tasks.second, outputCallback](LocalExecutorResult::Ptr localResult) {
                if (!localResult->m_result) {
                    outputCallback(std::make_shared<ToolProxyResponse>(localResult->m_stdOut));
                    UpdateRunningJobs(-1);
                    return;
                }
                if (m_rcClient.GetFreeRemoteThreads() > 0) {
                    auto inputFilename  = taskCC->m_invocation.GetInput();
                    auto remoteCallback = [this, outputCallback, inputFilename](const Wuild::RemoteToolClient::TaskExecutionInfo& info) {
                        FileInfo(inputFilename).Remove();
                        outputCallback(std::make_shared<ToolProxyResponse>(info.m_stdOutput, info.m_result));
                        UpdateRunningJobs(-1);
                    };
                    m_rcClient.InvokeTool(taskCC->m_invocation, remoteCallback);
                } else {
                    taskCC->m_callback = [this, outputCallback](LocalExecutorResult::Ptr localResult) {
                        outputCallback(std::make_shared<ToolProxyResponse>(localResult->m_stdOut, localResult->m_result));
                        UpdateRunningJobs(-1);
                    };
                    m_executor->AddTask(taskCC);
                }
            };
            m_executor->AddTask(taskPP);
        } else {
            original->m_callback = [outputCallback, this](LocalExecutorResult::Ptr localResult) {
                outputCallback(std::make_shared<ToolProxyResponse>(localResult->m_stdOut, localResult->m_result));
                UpdateRunningJobs(-1);
            };
            m_executor->AddTask(original);
        }
    }));
    m_runningJobsUpdate = TimePoint(true);

    m_server->Start();

    m_inactiveChecker.Exec([this, interruptCallback]() -> bool {
        std::lock_guard<std::mutex> lock(m_runningMutex);
        if (m_runningJobs == 0 && m_runningJobsUpdate.GetElapsedTime() > m_config.m_inactiveTimeout)
            interruptCallback();
        return true;
    },
                           100000 /*us*/);
}

void ToolProxyServer::UpdateRunningJobs(int delta)
{
    std::lock_guard<std::mutex> lock(m_runningMutex);
    m_runningJobsUpdate = TimePoint(true);
    m_runningJobs += delta;
}
}
