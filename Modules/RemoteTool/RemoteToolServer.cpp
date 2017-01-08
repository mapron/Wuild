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

#include <SocketFrameService.h>
#include <CoordinatorClient.h>
#include <ThreadUtils.h>

namespace Wuild
{

static const size_t g_recommendedBufferSize = 64 * 1024;

class RemoteToolServerImpl
{
public:
	std::mutex m_infoMutex;
	ToolServerInfo m_info;
	CoordinatorClient m_coordinator;
	std::unique_ptr<SocketFrameService> m_server;
	ILocalExecutor::Ptr m_executor;
	std::mutex m_sessionsIdsMutex;
	std::map<SocketFrameHandler*, int64_t> m_sessionsIds;
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
	ToolServerInfo & info = m_impl->m_info;
	info.m_connectionHost = m_config.m_listenHost;
	info.m_connectionPort = m_config.m_listenPort;
	info.m_totalThreads = m_config.m_threadCount;
	info.m_toolServerId = m_config.m_serverName;
	info.m_toolIds = m_impl->m_executor->GetToolIds();
	m_impl->m_executor->SetThreadCount(m_config.m_threadCount);

	m_impl->m_coordinator.SetToolServerInfo(info);
	if (!m_impl->m_coordinator.SetConfig(m_config.m_coordinator))
		return;

	SocketFrameHandlerSettings settings;
	settings.m_recommendedRecieveBufferSize = g_recommendedBufferSize;
	settings.m_recommendedSendBufferSize    = g_recommendedBufferSize;
	m_impl->m_server.reset(new SocketFrameService( settings, m_config.m_listenPort ));

	m_impl->m_server->SetHandlerInitCallback([this](SocketFrameHandler * handler){

		handler->RegisterFrameReader(SocketFrameReaderTemplate<RemoteToolRequest>::Create([this, handler](const RemoteToolRequest& inputMessage, SocketFrameHandler::OutputCallback outputCallback){

			const auto sessionId = inputMessage.m_sessionId;
			{
				std::lock_guard<std::mutex> lock(m_impl->m_sessionsIdsMutex);
				m_impl->m_sessionsIds[handler] = sessionId;
			}
			StartTask(inputMessage.m_clientId, sessionId);
			LocalExecutorTask::Ptr taskCC(new LocalExecutorTask());
			taskCC->m_invocation = inputMessage.m_invocation;
			taskCC->m_inputData = std::move(inputMessage.m_fileData);
			taskCC->m_callback = [outputCallback, this, sessionId](LocalExecutorResult::Ptr result)
			{
				FinishTask(sessionId, false);
				Syslogger() << "CC result = " << result->m_result ;
				RemoteToolResponse::Ptr response(new RemoteToolResponse());
				response->m_result = result->m_result;
				response->m_stdOut = result->m_stdOut;
				response->m_fileData = result->m_outputData;
				response->m_executionTime = result->m_executionTime;
				outputCallback(response);
			};
			m_impl->m_executor->AddTask(taskCC);
		}));
	});

	m_impl->m_server->SetHandlerDestroyCallback([this](SocketFrameHandler * handler){

		int64_t sessionId;
		{
			std::lock_guard<std::mutex> lock(m_impl->m_sessionsIdsMutex);
			sessionId = m_impl->m_sessionsIds[handler];
			m_impl->m_sessionsIds.erase(handler);
		}
		FinishTask(sessionId, true);
	});

	m_impl->m_server->Start();

	m_impl->m_coordinator.Start();
}

ToolServerInfo::ConnectedClientInfo & GetClientInfo(ToolServerInfo & info, int64_t sessionId)
{
	auto clientIt = std::find_if(info.m_connectedClients.begin(), info.m_connectedClients.end(), [sessionId](const auto & client){
		return client.m_sessionId == sessionId;
	});
	if (clientIt != info.m_connectedClients.end())
		return *clientIt;
	ToolServerInfo::ConnectedClientInfo client;
	client.m_sessionId = sessionId;
	info.m_connectedClients.push_back(client);
	return *info.m_connectedClients.rbegin();
}

void RemoteToolServer::StartTask(const std::string &clientId, int64_t sessionId)
{
	std::lock_guard<std::mutex> lock(m_impl->m_infoMutex);
	auto  &client = GetClientInfo(m_impl->m_info, sessionId);
	client.m_clientId = clientId;
	client.m_usedThreads ++;
	m_impl->m_coordinator.SetToolServerInfo(m_impl->m_info);
}

void RemoteToolServer::FinishTask(int64_t sessionId, bool remove)
{
	std::lock_guard<std::mutex> lock(m_impl->m_infoMutex);
	ToolServerInfo & info = m_impl->m_info;
	if (remove)
	{
		auto clientIt = std::find_if(info.m_connectedClients.begin(), info.m_connectedClients.end(), [sessionId](const auto & client){
			return client.m_sessionId == sessionId;
		});
		if (clientIt != info.m_connectedClients.end())
			info.m_connectedClients.erase(clientIt);
	}
	else
	{
		auto  &client = GetClientInfo(m_impl->m_info, sessionId);
		client.m_usedThreads --;
	}
	m_impl->m_coordinator.SetToolServerInfo(info);
}

}
