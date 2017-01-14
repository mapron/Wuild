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

#include "ToolBalancer.h"

#include <Syslogger.h>

#include <algorithm>
#include <assert.h>

namespace Wuild
{

ToolBalancer::ToolBalancer()
{

}

ToolBalancer::~ToolBalancer()
{

}

void ToolBalancer::SetRequiredTools(const StringVector &requiredToolIds)
{
	m_requiredToolIds = requiredToolIds;
}

void ToolBalancer::SetSessionId(int64_t sessionId)
{
	m_sessionId = sessionId;
}

ToolBalancer::ClientStatus ToolBalancer::UpdateClient(const ToolServerInfo &toolServer, size_t &index)
{
	if (!m_requiredToolIds.empty())
	{
		bool hasAtLeastOneTool = false;
		for (const auto & toolId : m_requiredToolIds)
		{
			if (std::find(toolServer.m_toolIds.begin(), toolServer.m_toolIds.end(), toolId) != toolServer.m_toolIds.end())
			{
				hasAtLeastOneTool = true;
				break;
			}
		}
		if (!hasAtLeastOneTool)
		{
			Syslogger() << "Skipping client "<< toolServer.m_connectionHost;
			return ClientStatus::Skipped;
		}
	}

	std::lock_guard<std::mutex> lock(m_clientsMutex);
	bool found = false;
	for (ClientInfo & clientsInfo : m_clients)
	{
		if (clientsInfo.m_toolServer.EqualIdTo(toolServer))
		{
			clientsInfo.m_toolServer = toolServer;
			clientsInfo.UpdateBusy(m_sessionId);
			found = true;
		}
	}
	if (found)
	{
		RecalcAvailable();
		return ClientStatus::Updated;
	}

	ClientInfo clientInfo;
	clientInfo.m_toolServer = toolServer;
	clientInfo.UpdateBusy(m_sessionId);
	m_clients.push_back(clientInfo);
	RecalcAvailable();
	return ClientStatus::Added;
}

void ToolBalancer::SetClientActive(size_t index, bool isActive)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);
	assert(index < m_clients.size());
	m_clients[index].m_active = isActive;
	RecalcAvailable();
}

size_t ToolBalancer::FindFreeClient(const std::string &toolId) const
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	int64_t minimalLoad = std::numeric_limits<int64_t>::max();
	size_t freeIndex = std::numeric_limits<size_t>::max();

	for (size_t index = 0; index < m_clients.size(); ++index)
	{
		const ClientInfo & client = m_clients[index];
		if (client.m_active)
		{
			const StringVector & toolIds = client.m_toolServer.m_toolIds;
			const bool toolExists = (std::find(toolIds.cbegin(), toolIds.cend(), toolId) != toolIds.cend());
			if (!toolExists)
				continue;
			int64_t clientLoad = (client.m_busyMine + client.m_busyOthers) * client.m_eachTaskWeight / client.m_toolServer.m_totalThreads;
			if (clientLoad < minimalLoad)
			{
				minimalLoad = clientLoad;
				freeIndex = index;
			}
		}
	}

	return freeIndex;
}

void ToolBalancer::StartTask(size_t index)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);
	m_clients[index].m_busyMine ++;
	RecalcAvailable();
}

void ToolBalancer::FinishTask(size_t index)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);
	uint16_t & busyMine = m_clients[index].m_busyMine;
	if (busyMine)
		--busyMine;
	RecalcAvailable();
}

void ToolBalancer::RecalcAvailable()
{
	//std::lock_guard<std::mutex> lock(m_clientsMutex);
	uint16_t free = 0;
	uint16_t used = 0;
	for (const ClientInfo & client : m_clients)
	{
		if (client.m_active)
		{
			auto busyOthers =  client.m_busyOthers;
			if (busyOthers)
				busyOthers--;
			free += client.m_toolServer.m_totalThreads - client.m_busyMine - busyOthers;
			used += client.m_busyMine;
		}
	}
	m_freeRemoteThreads = free;
	m_usedThreads = used;
}

void ToolBalancer::ClientInfo::UpdateBusy(int64_t mySessionId)
{
	m_busyMine = 0;
	m_busyOthers = 0;
	for (const ToolServerInfo::ConnectedClientInfo & client : m_toolServer.m_connectedClients)
	{
		if (!client.m_sessionId)
			continue;

		if (client.m_sessionId == mySessionId)
			m_busyMine += client.m_usedThreads;
		else
			m_busyOthers += client.m_usedThreads;
	}
}

}
