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

#include <CoordinatorClient.h>
#include <StringUtils.h>
#include <TimePoint.h>

#include <iostream>
#include <set>

int main(int argc, char** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "WuildProxyClient", "proxy");

	CoordinatorClient::Config config = app.m_remoteToolClientConfig.m_coordinator;
	if (!config.Validate())
		return 1;

	CoordinatorClient client;
	if (!client.SetConfig(config))
		return 1;


	client.SetInfoArrivedCallback([](const CoordinatorInfo& info){
		std::cout << "Coordinator connected tool servers: \n";

		std::map<int64_t, int> sessionsUsed;
		std::set<std::string> toolIds;
		int running = 0, thread = 0, queued = 0;

		for (const ToolServerInfo & toolServer : info.m_toolServers)
		{
			std::cout <<  toolServer.m_connectionHost << ":" << toolServer.m_connectionPort <<
						  " load:" << toolServer.m_runningTasks << "/" << toolServer.m_totalThreads << "\n";
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

		std::cout <<  "\nTotal load:" << running << "/" << thread << ", queue: " << queued << "\n";

		if (!sessionsUsed.empty())
		{
			std::cout << "\nRunning sessions:\n";
			for (const auto & s : sessionsUsed)
			{
				TimePoint stamp; stamp.SetUS(s.first); stamp.ToLocal();
				std::cout << "Started " << stamp.ToString() << ", used: " << s.second << "\n";
			}
		}

		std::cout << "\nAvailable tools: ";
		for (const auto & t : toolIds)
			std::cout << t << ", ";
		std::cout << "\n";
		Application::Interrupt(0);
	});

	client.Start();

	return ExecAppLoop();
}
