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

#include <iostream>

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
		for (const ToolServerInfo & toolServer : info.m_toolServers)
		{
			std::cout <<  toolServer.ToString(true, true) << "\n";
		}

		std::cout << "\n Coordinator current sessions: \n";
		for (const ToolServerSessionInfo & session : info.m_activeSessions)
		{
			std::cout <<  session.ToString(true, true) << "\n";
		}

		std::cout << "\n Coordinator latest finished sessions: \n";
		for (const ToolServerSessionInfo & session : info.m_latestSessions)
		{
			std::cout <<  session.ToString(false, true) << "\n";
		}

		Application::Interrupt(0);
	});

	client.Start();

	return ExecAppLoop();
}
