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

#include "ToolProxyClient.h"

#include "ToolProxyFrames.h"

#include <Syslogger.h>
#include <SocketFrameHandler.h>
#include <Application.h>
#include <FileUtils.h>

#include <algorithm>
#include <iostream>
#include <memory>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {
void StartDetached(const std::string& command)
{
    if (!Wuild::FileInfo(command).Exists())
        return;
#ifdef __APPLE__
    const std::string shell = "open -a " + command + " &";
    system(shell.c_str());
#elif !defined(_WIN32)
    const std::string shell = command + " &";
    if (system(shell.c_str()) < 0)
        Wuild::Syslogger(Wuild::Syslogger::Err) << "Failed to execute " << shell;
#endif
}
}

namespace Wuild {

ToolProxyClient::ToolProxyClient() = default;

ToolProxyClient::~ToolProxyClient()
{
    if (m_client)
        m_client->Stop();
}

bool ToolProxyClient::SetConfig(const ToolProxyClient::Config& config)
{
    std::ostringstream os;
    if (!config.Validate(&os)) {
        Syslogger(Syslogger::Err) << os.str();
        return false;
    }
    m_config = config;
    return true;
}

bool ToolProxyClient::Start()
{
    SocketFrameHandlerSettings settings;
    settings.m_writeFailureLogLevel = Syslogger::Info;
    m_client                        = std::make_unique<SocketFrameHandler>(settings);
    m_client->RegisterFrameReader(SocketFrameReaderTemplate<ToolProxyResponse>::Create());

    m_client->SetTcpChannel("localhost", m_config.m_listenPort);
    m_client->SetChannelNotifier([this](bool state) {
        std::unique_lock<std::mutex> lock(m_connectionStateMutex);
        m_connectionState = state;
        m_connectionStateCond.notify_one();
    });
    m_client->Start();

    std::unique_lock<std::mutex> lock(m_connectionStateMutex);
    std::chrono::milliseconds    processRand(0);
#ifndef _WIN32
    // additional sleep for 1..100 ms depending on process id.
    srand(getpid());
    processRand = std::chrono::milliseconds(1 + rand() % 100);
#endif

    m_connectionStateCond.wait_for(lock, processRand + std::chrono::microseconds(m_config.m_clientConnectionTimeout.GetUS()), [this] {
        return !!m_connectionState;
    });

    if (!m_connectionState) {
        StartDetached(m_config.m_startCommand);
    }

    m_connectionStateCond.wait_for(lock, processRand + std::chrono::microseconds(m_config.m_clientConnectionTimeout.GetUS()), [this] {
        return !!m_connectionState;
    });

    return m_connectionState;
}

void ToolProxyClient::RunTask(const StringVector& args)
{
    auto frameCallback = [](SocketFrame::Ptr responseFrame, SocketFrameHandler::ReplyState state, const std::string& errorInfo) {
        std::string stdOut;
        bool        result = false;
        if (state == SocketFrameHandler::ReplyState::Timeout) {
            stdOut = "Timeout expired:" + errorInfo;
        } else if (state == SocketFrameHandler::ReplyState::Error) {
            stdOut = "Internal error.";
        } else {
            ToolProxyResponse::Ptr responseFrameProxy = std::dynamic_pointer_cast<ToolProxyResponse>(responseFrame);
            result                                    = responseFrameProxy->m_result;
            stdOut                                    = responseFrameProxy->m_stdOut;
        }
        std::replace(stdOut.begin(), stdOut.end(), '\r', ' ');
        if (!stdOut.empty())
            std::cerr << stdOut << std::endl
                      << std::flush;
        Application::Interrupt(1 - result);
    };
    ToolProxyRequest::Ptr req(new ToolProxyRequest());
    req->m_invocation.m_id.m_toolId = m_config.m_toolId;
    req->m_invocation.m_args        = args;
    req->m_cwd                      = GetCWD();
    m_client->QueueFrame(req, frameCallback, m_config.m_proxyClientTimeout);
}

}
