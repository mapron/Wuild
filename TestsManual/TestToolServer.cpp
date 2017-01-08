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

#include <RemoteToolClient.h>
#include <RemoteToolServer.h>
#include <LocalExecutor.h>

const int g_toolsServerTestPort = 12345;

int main(int argc, char** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "TestToolServer");
	if (!CreateInvocationRewriter(app))
	   return 1;

	StringVector args = app.GetRemainArgs();
	auto localExecutor = LocalExecutor::Create(TestConfiguration::s_invocationRewriter, app.m_tempDir);

	std::string err;
	LocalExecutorTask::Ptr original(new LocalExecutorTask());
	original->m_readOutput = original->m_writeInput = false;
	original->m_invocation = ToolInvocation( args ).SetExecutable(TestConfiguration::s_invocationRewriter->GetConfig().GetFirstToolName());
	auto tasks = localExecutor->SplitTask(original, err);
	if (!tasks.first)
	{
		Syslogger(LOG_ERR) << err;
		return 1;
	}
	LocalExecutorTask::Ptr taskPP = tasks.first;
	LocalExecutorTask::Ptr taskCC = tasks.second;

	RemoteToolServer::Config toolServerConfig;
	toolServerConfig.m_listenHost = "localhost";
	toolServerConfig.m_listenPort = g_toolsServerTestPort;
	toolServerConfig.m_coordinator.m_enabled = false;

	RemoteToolServer rcServer(localExecutor);
	if (!rcServer.SetConfig(toolServerConfig))
		return 1;

	rcServer.Start();

	RemoteToolClient rcClient;
	ToolServerInfo toolServerInfo;
	toolServerInfo.m_connectionHost = "localhost";
	toolServerInfo.m_connectionPort = g_toolsServerTestPort;
	toolServerInfo.m_toolIds = TestConfiguration::s_invocationRewriter->GetConfig().m_toolIds;
	toolServerInfo.m_totalThreads = 1;
	rcClient.AddClient(toolServerInfo);
	RemoteToolClient::Config clientConfig;
	clientConfig.m_coordinator.m_enabled = false;
	rcClient.SetConfig(clientConfig);
	rcClient.Start();


	taskPP->m_callback = [&rcClient, taskCC] ( LocalExecutorResult::Ptr localResult ) {

		if (!localResult->m_result)
		{
			Syslogger(LOG_ERR) << "Preprocess failed";
			Application::Interrupt(1);
			return;
		}

		auto callback = []( const Wuild::RemoteToolClient::ExecutionInfo& info){
			if (info.m_stdOutput.size())
				std::cerr << info.m_stdOutput << std::endl << std::flush;
			std::cout << info.GetProfilingStr() << " \n";
			Application::Interrupt(1 - info.m_result);
		};
		rcClient.InvokeTool(taskCC->m_invocation, callback);
	};
	localExecutor->AddTask(taskPP);

	return ExecAppLoop(TestConfiguration::ExitHandler);
}
