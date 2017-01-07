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

const int g_workerTestPort = 12345;

int main(int argc, char** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "TestWorker");
	if (!CreateCompiler(app))
	   return 1;

	StringVector args = app.GetRemainArgs();
	auto localExecutor = LocalExecutor::Create(TestConfiguration::g_compilerModule, app.m_tempDir);

	std::string err;
	LocalExecutorTask::Ptr original(new LocalExecutorTask());
	original->m_readOutput = original->m_writeInput = false;
	original->m_invocation = CompilerInvocation( args ).SetExecutable(TestConfiguration::g_compilerModule->GetConfig().GetFirstToolName());
	auto tasks = localExecutor->SplitTask(original, err);
	if (!tasks.first)
	{
		Syslogger(LOG_ERR) << err;
		return 1;
	}
	LocalExecutorTask::Ptr taskPP = tasks.first;
	LocalExecutorTask::Ptr taskCC = tasks.second;

	RemoteToolServer::Config workerConfig;
	workerConfig.m_listenHost = "localhost";
	workerConfig.m_listenPort = g_workerTestPort;
	workerConfig.m_coordinator.m_enabled = false;

	RemoteToolServer rcServer(localExecutor);
	if (!rcServer.SetConfig(workerConfig))
		return 1;

	rcServer.Start();

	RemoteToolClient rcClient;
	CoordinatorWorkerInfo workerInfo;
	workerInfo.m_connectionHost = "localhost";
	workerInfo.m_connectionPort = g_workerTestPort;
	workerInfo.m_toolIds = TestConfiguration::g_compilerModule->GetConfig().m_toolIds;
	workerInfo.m_totalThreads = 1;
	rcClient.AddClient(workerInfo);
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
