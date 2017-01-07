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

#include "RemoteToolServer.h"

#include "RemoteToolFrames.h"
#include "SocketFrameService.h"
#include "CoordinatorClient.h"

#include <ThreadUtils.h>

namespace Wuild
{

static const size_t g_recommendedBufferSize = 64 * 1024;

class RemoteToolServerImpl
{
public:
	CoordinatorWorkerInfo m_info;
	CoordinatorClient m_coordinator;
	std::unique_ptr<SocketFrameService> m_server;
	ILocalExecutor::Ptr m_executor;
	std::map<SocketFrameHandler*, WorkerSessionInfo> m_sessions;
	std::atomic_int_least64_t m_sessionIdCounter { 0 };
	void SendSessionInfo(const WorkerSessionInfo & info);
};

RemoteToolServer::RemoteToolServer(ILocalExecutor::Ptr executor)
	: m_impl(new RemoteToolServerImpl())
{
	 m_impl->m_executor = executor;
}

RemoteToolServer::~RemoteToolServer()
{
	m_impl->m_server.reset();
}

bool RemoteToolServer::SetConfig(const RemoteToolServer::Config &config)
{
	std::ostringstream os;
	if (!config.Validate(&os))
	{
		Syslogger(LOG_ERR) << os.str();
		return false;
	}
	m_config = config;
	return true;
}

void RemoteToolServer::Start()
{
	CoordinatorWorkerInfo & info = m_impl->m_info;
	info.m_connectionHost = m_config.m_listenHost;
	info.m_connectionPort = m_config.m_listenPort;
	info.m_totalThreads = m_config.m_workersCount;
	info.m_workerId = m_config.m_workerId;
	info.m_toolIds = m_impl->m_executor->GetToolIds();
	m_impl->m_executor->SetWorkersCount(m_config.m_workersCount);

	m_impl->m_coordinator.SetWorkerInfo(info);
	if (!m_impl->m_coordinator.SetConfig(m_config.m_coordinator))
		return;

	SocketFrameHandlerSettings settings;
	settings.m_recommendedRecieveBufferSize = g_recommendedBufferSize;
	m_impl->m_server.reset(new SocketFrameService( settings, m_config.m_listenPort ));

	m_impl->m_server->SetHandlerInitCallback([this](SocketFrameHandler * handler){

		m_impl->m_sessions[handler].m_sessionId = m_impl->m_sessionIdCounter++;

		handler->RegisterFrameReader(SocketFrameReaderTemplate<RemoteToolRequest>::Create([this, handler](const RemoteToolRequest& inputMessage, SocketFrameHandler::OutputCallback outputCallback){

			LocalExecutorTask::Ptr taskCC(new LocalExecutorTask());
			taskCC->m_invocation = inputMessage.m_invocation;
			taskCC->m_inputData = std::move(inputMessage.m_fileData);
			taskCC->m_callback = [outputCallback, this, handler](LocalExecutorResult::Ptr result)
			{
				Syslogger() << "CC result = " << result->m_result ;
				RemoteToolResponse::Ptr response(new RemoteToolResponse());
				response->m_result = result->m_result;
				response->m_stdOut = result->m_stdOut;
				response->m_fileData = result->m_outputData;
				response->m_executionTime = result->m_executionTime;
				outputCallback(response);
				{
					WorkerSessionInfo & sessionInfo = m_impl->m_sessions[handler];
					sessionInfo.m_totalExecutionTime += result->m_executionTime;
					sessionInfo.m_tasksCount++;
					if (!result->m_result)
						sessionInfo.m_failuresCount++;
				}
			};
			m_impl->m_executor->AddTask(taskCC);
		}));
	});

	m_impl->m_server->SetHandlerDestroyCallback([this](SocketFrameHandler * handler){

		WorkerSessionInfo sessionInfo = m_impl->m_sessions[handler];
		m_impl->m_sessions.erase(handler);
		m_impl->SendSessionInfo(sessionInfo);
	});

	m_impl->m_server->Start();

	m_impl->m_coordinator.Start();
}

void RemoteToolServerImpl::SendSessionInfo(const WorkerSessionInfo &info)
{
	if (!info.m_tasksCount)
		return;

	Syslogger() << "Finished " << info.ToString();
	m_coordinator.SendWorkerSessionInfo(info);
}


}
