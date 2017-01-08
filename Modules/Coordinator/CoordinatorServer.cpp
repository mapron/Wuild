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

#include "CoordinatorServer.h"

#include "CoordinatorFrames.h"

#include <SocketFrameService.h>
#include <ThreadUtils.h>

namespace Wuild
{

CoordinatorServer::CoordinatorServer()
{
}

CoordinatorServer::~CoordinatorServer()
{
	m_server.reset();
}

bool CoordinatorServer::SetConfig(const CoordinatorServer::Config &config)
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


void CoordinatorServer::Start()
{
	m_server.reset(new SocketFrameService( m_config.m_listenPort ));
	//m_server->RegisterFrameReader();

	m_server->RegisterFrameReader(SocketFrameReaderTemplate<CoordinatorListRequest>::Create([this](const CoordinatorListRequest& inputMessage, SocketFrameHandler::OutputCallback outputCallback){
		(void)inputMessage;
		outputCallback(GetResponse());
	}));

	m_server->SetHandlerInitCallback([this](SocketFrameHandler * handler){

		handler->RegisterFrameReader(SocketFrameReaderTemplate<CoordinatorToolServerSession>::Create([this, handler](const CoordinatorToolServerSession& inputMessage, SocketFrameHandler::OutputCallback){
			std::lock_guard<std::mutex> lock(m_infoMutex);
			if (inputMessage.m_isFinished)
			{
				m_info.m_latestSessions.push_back(inputMessage.m_session);
				if ((int)m_info.m_latestSessions.size() > m_config.m_lastestSessionsSize)
					m_info.m_latestSessions.pop_front();

				auto activeSessionIt =  m_info.m_activeSessions.begin();
				for (;activeSessionIt != m_info.m_activeSessions.end(); ++activeSessionIt )
				{
				   if ( activeSessionIt->m_sessionId == inputMessage.m_session.m_sessionId)
				   {
					   m_info.m_activeSessions.erase(activeSessionIt);
					   return;
				   }
				}
			}
			else
			{
				for (ToolServerSessionInfo & session : m_info.m_activeSessions)
				{
					if (session.m_sessionId == inputMessage.m_session.m_sessionId)
					{
						session = inputMessage.m_session;
						return;
					}
				}
				m_info.m_activeSessions.push_back(inputMessage.m_session);
			}
		}));

		handler->RegisterFrameReader(SocketFrameReaderTemplate<CoordinatorToolServerStatus>::Create([this, handler](const CoordinatorToolServerStatus& inputMessage, SocketFrameHandler::OutputCallback ){

			std::vector<ToolServerInfo*> modified;
			{
				std::lock_guard<std::mutex> lock(m_infoMutex);
				modified = m_info.Update(inputMessage.m_info);
			}
			if (!modified.empty())
			{
				modified[0]->m_opaqueFrameHandler = handler;
				m_server->QueueFrameToAll(handler, GetResponse());
			}
		}));

		handler->QueueFrame(GetResponse());
	});

	m_server->SetHandlerDestroyCallback([this](SocketFrameHandler * handler){
		std::lock_guard<std::mutex> lock(m_infoMutex);
		auto toolServerIt =  m_info.m_toolServers.begin();
		for (;toolServerIt != m_info.m_toolServers.end(); ++toolServerIt )
		{
		   if ( toolServerIt->m_opaqueFrameHandler == handler)
		   {
			   m_info.m_toolServers.erase(toolServerIt);
			   break;
		   }
		}
		auto activeSessionIt =  m_info.m_activeSessions.begin();
		for (;activeSessionIt != m_info.m_activeSessions.end(); ++activeSessionIt )
		{
		   if ( activeSessionIt->m_opaqueFrameHandler == handler)
		   {
			   m_info.m_activeSessions.erase(activeSessionIt);
			   break;
		   }
		}
		// do not send info update immediately, no need.
	});
	m_server->Start();
}

std::shared_ptr<CoordinatorListResponse> CoordinatorServer::GetResponse()
{
	CoordinatorListResponse::Ptr infoList(new CoordinatorListResponse());
	{
		std::lock_guard<std::mutex> lock(m_infoMutex);
		infoList->m_info = m_info;
	}
	return infoList;
}

}
