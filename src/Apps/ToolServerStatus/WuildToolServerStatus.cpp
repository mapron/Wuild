/*
 * Copyright (C) 2021 Smirnov Vladimir mapron1@gmail.com
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
#include "StatusWriter.h"

#include <ArgStorage.h>
#include <CoordinatorClient.h>
#include <RemoteToolFrames.h>
#include <SocketFrameHandler.h>
#include <StringUtils.h>
#include <TimePoint.h>
#include <VersionChecker.h>
#include <LocalExecutor.h>

#include <iostream>

namespace Wuild {

class RemoteToolVersionClient {
public:
    using VersionMap = std::map<std::string, std::string>;

public:
    RemoteToolVersionClient(AbstractWriter& writer, TimePoint requestTimeout)
        : m_writer(writer)
        , m_requestTimeout(requestTimeout)
    {
    }

    ~RemoteToolVersionClient()
    {
        std::map<std::string, std::map<std::string, std::string>> conflictedVersions;
        for (const auto& tool : m_allVersionsByToolId) {
            const auto& id = tool.first;
            // Complexity O(N^2) here, N = number of hosts. Hosts is usually reasonably low count, so not a problem yet.
            for (const auto& host1 : tool.second) {
                for (const auto& host2 : tool.second) {
                    if (host1.second != host2.second)
                        conflictedVersions[id][host1.first] = host1.second;
                }
            }
        }
        m_writer.FormatToolsConflicts(conflictedVersions);
    }

    void AddClient(const ToolServerInfo& info)
    {
        SocketFrameHandlerSettings settings;
        settings.m_channelProtocolVersion = RemoteToolRequest::s_version + RemoteToolResponse::s_version;
        settings.m_segmentSize            = 8192;
        settings.m_hasConnStatus          = true;
        SocketFrameHandler::Ptr handler(new SocketFrameHandler(settings));
        handler->RegisterFrameReader(SocketFrameReaderTemplate<ToolsVersionResponse>::Create());
        handler->SetTcpChannel(info.m_connectionHost, info.m_connectionPort);
        ++m_toolVersionClientWaitCounter;

        auto versionFrameCallback = [this, info](SocketFrame::Ptr responseFrame, SocketFrameHandler::ReplyState state, const std::string& errorInfo) {
            if (state == SocketFrameHandler::ReplyState::Timeout || state == SocketFrameHandler::ReplyState::Error) {
                Syslogger(Syslogger::Err) << "Error on requesting toolserver " << info.m_connectionHost << ", " << errorInfo;
            } else {
                ToolsVersionResponse::Ptr result = std::dynamic_pointer_cast<ToolsVersionResponse>(responseFrame);
                ProcessToolsVersions(info.m_connectionHost + ":" + std::to_string(info.m_connectionPort), result->m_versions);

                if (--m_toolVersionClientWaitCounter == 0)
                    Application::Interrupt(0);
            }
        };

        handler->QueueFrame(ToolsVersionRequest::Ptr(new ToolsVersionRequest()), versionFrameCallback, m_requestTimeout);
        m_clients.push_back(handler);
        handler->Start();
    }

    void ProcessToolsVersions(const std::string& host, const VersionMap& versionByToolId)
    {
        std::unique_lock lock(m_writerMutex);
        m_writer.FormatToolsVersions(host, versionByToolId);
        for (const auto& tool : versionByToolId)
            m_allVersionsByToolId[tool.first][host] = tool.second;
    }

private:
    AbstractWriter&                                           m_writer;
    TimePoint                                                 m_requestTimeout;
    std::deque<SocketFrameHandler::Ptr>                       m_clients;
    std::atomic_size_t                                        m_toolVersionClientWaitCounter{ 0 };
    std::mutex                                                m_writerMutex;
    std::map<std::string, std::map<std::string, std::string>> m_allVersionsByToolId;
};

}

// Usage: WuildToolServerStatus [--json] [--show-local-tools] [<filter toolserver hostname>]
int main(int argc, char** argv)
{
    using namespace Wuild;
    ArgStorage            argStorage(argc, argv);
    ConfiguredApplication app(argStorage.GetConfigValues(), "WuildToolServerStatus");

    CoordinatorClient::Config config = app.m_remoteToolClientConfig.m_coordinator;
    if (!config.Validate())
        return 1;

    CoordinatorClient client;
    if (!client.SetConfig(config))
        return 1;

    bool                    showLocalTools = false;
    AbstractWriter::OutType outType        = AbstractWriter::OutType::STD_TEXT;
    std::string             toolServerFilter;
    for (auto&& arg : argStorage.GetArgs())
        if (arg == "--json")
            outType = AbstractWriter::OutType::JSON;
        else if (arg == "--show-local-tools")
            showLocalTools = true;
        else
            toolServerFilter = arg;

    std::unique_ptr<AbstractWriter> writer = AbstractWriter::AbstractWriter::createWriter(outType, std::cout);

    RemoteToolVersionClient toolVersionClient(*writer, app.m_remoteToolClientConfig.m_requestTimeout);

    writer->FormatMessage("Tool versions by toolserver:\n");

    if (showLocalTools) {
        auto invocationRewriter = CheckedCreateInvocationRewriter(app);
        if (!invocationRewriter)
            return 1;

        auto localExecutor = LocalExecutor::Create(invocationRewriter, app.m_tempDir);

        auto       versionChecker = VersionChecker::Create(localExecutor, invocationRewriter);
        const auto toolsVersions  = versionChecker->DetermineToolVersions({});

        toolVersionClient.ProcessToolsVersions("localhost", toolsVersions);
    }

    client.SetInfoArrivedCallback([&](const CoordinatorInfo& info) {
        bool hasServers = false;
        for (const ToolServerInfo& toolServer : info.m_toolServers) {
            if (!toolServerFilter.empty() && toolServer.m_connectionHost != toolServerFilter)
                continue;

            toolVersionClient.AddClient(toolServer);
            hasServers = true;
        }

        if (!hasServers)
            Application::Interrupt(0);
    });

    client.Start();

    return ExecAppLoop();
}
