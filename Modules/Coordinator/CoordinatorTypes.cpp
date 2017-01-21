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

#include "CoordinatorTypes.h"
#include <sstream>

namespace Wuild
{

std::string ToolServerInfo::ToString(bool outputTools, bool outputClients) const
{
	std::ostringstream os;
	os <<  m_connectionHost << ":" << m_connectionPort << " (" << m_toolServerId << "),"
		<< " threads: " << m_totalThreads
		<< " queue: " << m_queuedTasks
		<< " running: " << m_runningTasks
		   ;
	if (outputTools)
	{
		os << " Tools: ";
		  for (const std::string & t : m_toolIds)
			  os  << t << ", ";
	}
	if (outputClients)
	{
		os << " Connected: " << m_connectedClients.size();
		for (const ConnectedClientInfo & c : m_connectedClients)
			 os << "sid=" << c.m_sessionId << " (" <<  c.m_clientId << "): use=" << c.m_usedThreads  << ", ";
	}
	return os.str();
}

bool ToolServerInfo::EqualIdTo(const ToolServerInfo &rh) const
{
	return true
			&& m_toolServerId == rh.m_toolServerId
			&& m_connectionHost == rh.m_connectionHost
			&& m_connectionPort == rh.m_connectionPort
			;
}

bool ToolServerInfo::ConnectedClientInfo::operator ==(const ToolServerInfo::ConnectedClientInfo &rh) const
{
	return true
			&& m_usedThreads == rh.m_usedThreads
			&& m_clientId == rh.m_clientId
			&& m_sessionId == rh.m_sessionId
			;
}

bool ToolServerInfo::operator ==(const ToolServerInfo &rh) const
{
	return true
			&& EqualIdTo(rh)
			&& m_toolIds == rh.m_toolIds
			&& m_totalThreads == rh.m_totalThreads
			&& m_connectedClients == rh.m_connectedClients
			;
}

std::vector<ToolServerInfo *> CoordinatorInfo::Update(const ToolServerInfo &newToolServer)
{
	return Update(std::deque<ToolServerInfo>(1, newToolServer));
}

std::vector<ToolServerInfo *> CoordinatorInfo::Update(const std::deque<ToolServerInfo> &newNoolServers)
{
	std::vector<ToolServerInfo *> res;

	for (const ToolServerInfo &newToolServer : newNoolServers)
	{
		if (newToolServer.m_connectionHost.empty())
			continue;

		bool found = false;
		for (auto & toolServerInfo : m_toolServers)
		{
		   if (toolServerInfo.EqualIdTo(newToolServer))
		   {
			   if (toolServerInfo != newToolServer)
			   {
				   res.push_back(&toolServerInfo);
				   toolServerInfo = newToolServer;
			   }
			   found = true;
			   break;
		   }
		}
		if (!found)
		{
			m_toolServers.push_back(newToolServer);
			res.push_back(&*m_toolServers.rbegin());
		}
	}
	return res;
}

std::string CoordinatorInfo::ToString(bool outputTools, bool outputClients) const
{
	std::ostringstream os;
	os << "Total tool servers: " << m_toolServers.size();
	for (const auto & w : m_toolServers)
		os << "\n" << w.ToString(outputTools, outputClients);
	return os.str();
}

bool CoordinatorInfo::operator ==(const CoordinatorInfo &rh) const
{
	return m_toolServers == rh.m_toolServers;
}

std::string ToolServerSessionInfo::ToString(bool asCurrent, bool full) const
{
	std::ostringstream os;
	os  << "sid:" << m_sessionId;
	if (!m_clientId.empty())
		os << ", client:" << m_clientId ;

	if (asCurrent)
		os << ", tasks:" << m_tasksCount
		   ;
	if (full)
		os << "(err:" << m_failuresCount << ")";

	if (asCurrent)
	{
		os << ", usedThreads:" << m_currentUsedThreads;
	}

	if (!asCurrent)
	{
		os << ", maxThreads:" << m_maxUsedThreads;
		auto cus = m_totalExecutionTime.GetUS();
		auto nus = m_totalNetworkTime.GetUS();
		const std::string execstr = m_totalExecutionTime.ToProfilingTime();
		const std::string netstr  = m_totalNetworkTime  .ToProfilingTime();
		auto overheadPercent = ((nus - cus) * 100) / (cus ? cus : 1);
		os << " Total remote tasks:" << m_tasksCount << ", done in " << m_elapsedTime.ToString(false)
							  <<  " total compilationTime: " << execstr << ", "
							   << " total networkTime: "  << netstr<< ", "
							   << " total overhead: " << overheadPercent << "%"
								  ;


	}
	return os.str();
}


}
