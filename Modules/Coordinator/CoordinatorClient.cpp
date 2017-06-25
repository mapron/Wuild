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

#include "CoordinatorClient.h"

#include "CoordinatorFrames.h"

#include <SocketFrameService.h>
#include <ThreadUtils.h>

#include <stdio.h>
#include <stdlib.h>
#include <functional>
#include <fstream>

namespace Wuild
{

CoordinatorClient::CoordinatorClient()
{
}

CoordinatorClient::~CoordinatorClient()
{

}

bool CoordinatorClient::SetConfig(const CoordinatorClient::Config &config)
{
	std::ostringstream os;
	if (!config.Validate(&os))
	{
		Syslogger(Syslogger::Err) << os.str();
		return false;
	}
	m_config = config;
	return true;
}

void CoordinatorClient::SetInfoArrivedCallback(CoordinatorClient::InfoArrivedCallback callback)
{
	m_infoArrivedCallback = callback;
}

void CoordinatorClient::Start()
{
	if (!m_config.m_enabled || m_config.m_coordinatorPort <= 0)
		return;

	m_workers.clear();

	for (const auto & host : m_config.m_coordinatorHost)
	{
		auto worker = std::make_shared<CoordWorker>();
		worker->m_coordClient = this;
		worker->SetToolServerInfo(m_lastInfo);
		worker->Start(host, m_config.m_coordinatorPort);
		m_workers.emplace_back(worker);
	}
}

void CoordinatorClient::SetToolServerInfo(const ToolServerInfo &info)
{
	m_lastInfo = info;
	for (auto & worker : m_workers)
		worker->SetToolServerInfo(info);
}

void CoordinatorClient::SendToolServerSessionInfo(const ToolServerSessionInfo &sessionInfo, bool isFinished)
{
	if (m_workers.empty())
		return;

	Syslogger(this->m_config.m_logContext) << " sending session " <<  sessionInfo.m_clientId;

	for (auto & worker : m_workers)
	{
		if (!worker->m_clientState)
			continue;

		CoordinatorToolServerSession::Ptr message(new CoordinatorToolServerSession());
		message->m_isFinished = isFinished;
		message->m_session = sessionInfo;
		worker->m_client->QueueFrame(message);
	}
}

void CoordinatorClient::StopExtraClients(const std::string &hostExcept)
{
	if (m_exclusiveModeSet || m_config.m_redundance != CoordinatorClientConfig::Redundance::Any)
		return;

	m_exclusiveModeSet = true;
	for (auto & worker : m_workers)
	{
		if (worker->m_host != hostExcept)
		{
			Syslogger(Syslogger::Info) << "Stopping unused coordinator: " << worker->m_host;
			worker->Stop();
		}
	}
}

void CoordinatorClient::CoordWorker::SetToolServerInfo(const ToolServerInfo &info)
{
	std::lock_guard<std::mutex> lock(m_toolServerInfoMutex);
	if (m_toolServerInfo == info)
		return;

	m_needSendToolServerInfo = true;

	m_toolServerInfo = info;
}

void CoordinatorClient::CoordWorker::Quant()
{
	if (!m_clientState)
		return;

	if (m_coordClient->m_config.m_sendInfoInterval && m_needSendToolServerInfo)
	{
		if (!m_lastSend || m_lastSend.GetElapsedTime() > m_coordClient->m_config.m_sendInfoInterval)
		{
			m_lastSend = TimePoint(true);
			m_needSendToolServerInfo = false;
			{
				std::lock_guard<std::mutex> lock(m_toolServerInfoMutex);
				if (m_toolServerInfo.m_totalThreads)
				{
					Syslogger(m_coordClient->m_config.m_logContext) << " sending tool server " <<  m_toolServerInfo.m_toolServerId;
					CoordinatorToolServerStatus::Ptr message(new CoordinatorToolServerStatus());
					message->m_info = m_toolServerInfo;
					m_client->QueueFrame(message);
				}
			}
		}
	}
	if (m_needRequestData)
	{
		m_needRequestData = false;
		m_client->QueueFrame(CoordinatorListRequest::Ptr(new CoordinatorListRequest()));
	}
}

void CoordinatorClient::CoordWorker::Stop()
{
	m_clientState = false;
	m_client->Stop();
	m_thread.Stop();
}

void CoordinatorClient::CoordWorker::Start(const std::string &host, int port)
{
	m_host = host;
	SocketFrameHandlerSettings settings;
	settings.m_channelProtocolVersion   = CoordinatorListRequest::s_version
										+ CoordinatorListResponse::s_version
										+ CoordinatorToolServerStatus::s_version
										+ CoordinatorToolServerSession::s_version
										;

	m_client.reset(new SocketFrameHandler( settings ));
	m_client->RegisterFrameReader(SocketFrameReaderTemplate<CoordinatorListResponse>::Create([this, host](const CoordinatorListResponse& inputMessage, SocketFrameHandler::OutputCallback){
		 std::lock_guard<std::mutex> lock(m_coordClient->m_coordMutex);
		 m_coordClient->StopExtraClients(host);
		 Syslogger(m_coordClient->m_config.m_logContext) << " list arrived [" << inputMessage.m_info.m_toolServers.size() << "]";
		 auto modified = m_coordClient->m_coord.Update(inputMessage.m_info.m_toolServers);
		 if (!modified.empty())
		 {
			 if (m_coordClient->m_infoArrivedCallback)
				 m_coordClient->m_infoArrivedCallback(m_coordClient->m_coord);
		 }
	}));
	m_client->SetChannelNotifier([this](bool state){
		m_clientState = state;
		if (!state)
		{
			m_needRequestData = true;
			m_needSendToolServerInfo = true;
		}
	});

	m_client->SetTcpChannel(host, port);

	m_client->Start();
	m_thread.Exec(std::bind(&CoordinatorClient::CoordWorker::Quant, this));
}

CoordinatorClient::CoordWorker::~CoordWorker()
{
	Stop();
}

}
