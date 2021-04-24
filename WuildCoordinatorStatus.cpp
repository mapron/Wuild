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
#include "StatusWriter.h"

#include <ArgStorage.h>
#include <CoordinatorClient.h>
#include <StringUtils.h>
#include <TimePoint.h>

int main(int argc, char** argv)
{
	using namespace Wuild;
	ArgStorage argStorage(argc, argv);
	ConfiguredApplication app(argStorage.GetConfigValues(), "WuildProxyClient", "proxy");

	CoordinatorClient::Config config = app.m_remoteToolClientConfig.m_coordinator;
	if (!config.Validate())
		return 1;

	CoordinatorClient client;
	if (!client.SetConfig(config))
		return 1;

	AbstractWriter::OutType outType = AbstractWriter::OutType::STD_TEXT;
	for (auto arg : argStorage.GetArgs())
		if (!arg.compare("--json"))
			outType = AbstractWriter::OutType::JSON;
	std::unique_ptr<AbstractWriter> writer = AbstractWriter::AbstractWriter::createWriter(outType);

	client.SetInfoArrivedCallback([&writer](const CoordinatorInfo& info){
		writer->PrintMessage("Coordinator connected tool servers:\n");

		std::map<int64_t, int> sessionsUsed;
		std::set<std::string> toolIds;
		int running = 0, thread = 0, queued = 0;

		for (const ToolServerInfo & toolServer : info.m_toolServers)
		{
			writer->PrintConnectedToolServer(toolServer.m_connectionHost, toolServer.m_connectionPort,
											 toolServer.m_runningTasks, toolServer.m_totalThreads);
			running += toolServer.m_runningTasks;
			queued += toolServer.m_queuedTasks;
			thread += toolServer.m_totalThreads;
			for (const ToolServerInfo::ConnectedClientInfo & client : toolServer.m_connectedClients)
			{
				sessionsUsed[client.m_sessionId] += client.m_usedThreads ;
			}
			for (const std::string & t : toolServer.m_toolIds)
				toolIds.insert(t);
		}

		writer->PrintSummary(running, thread, queued);

		if (!sessionsUsed.empty())
		{
			writer->PrintMessage("\nRunning sessions:\n");
			for (const auto & s : sessionsUsed)
			{
				TimePoint stamp; stamp.SetUS(s.first); stamp.ToLocal();
				writer->PrintSessions(stamp.ToString(), s.second);
			}
		}
		writer->PrintTools(toolIds);
		Application::Interrupt(0);
	});

	client.Start();

	return ExecAppLoop();
}

