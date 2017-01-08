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

#pragma once

#include <TimePoint.h>
#include <CommonTypes.h>

#include <deque>

namespace Wuild
{
/// Information about remote tool server state.
struct ToolServerInfo
{
	std::string m_toolServerId;
	std::string m_connectionHost;
	int16_t m_connectionPort = 0;
	StringVector m_toolIds;
	uint16_t m_totalThreads = 0;

	struct ConnectedClientInfo
	{
		uint16_t m_usedThreads = 0;
		std::string m_clientId;
		int64_t m_sessionId = 0;

		bool operator ==(const ConnectedClientInfo& rh) const;
		bool operator !=(const ConnectedClientInfo& rh) const { return !(*this == rh);}
	};
	std::vector<ConnectedClientInfo> m_connectedClients;
	std::string ToString(bool outputTools = false, bool outputClients = false) const;
	bool EqualIdTo(const ToolServerInfo & rh) const;

	void *m_opaqueFrameHandler = nullptr;

	bool operator ==(const ToolServerInfo& rh) const;
	bool operator !=(const ToolServerInfo& rh) const { return !(*this == rh);}
};

/// Information about finished compilation session (sequence of tool executions)
struct ToolServerSessionInfo
{
	using List = std::deque<ToolServerSessionInfo>;
	std::string m_clientId;
	int64_t m_sessionId;
	TimePoint m_totalNetworkTime;
	TimePoint m_totalExecutionTime;
	TimePoint m_elapsedTime;
	uint32_t m_tasksCount = 0;
	uint32_t m_failuresCount = 0;
	uint32_t m_currentUsedThreads = 0;
	uint32_t m_maxUsedThreads = 0;

	void *m_opaqueFrameHandler = nullptr;

	std::string ToString(bool asCurrent, bool full = false) const;
};

/// Full coordinator data
struct CoordinatorInfo
{
	std::deque<ToolServerInfo>   m_toolServers;
	ToolServerSessionInfo::List  m_latestSessions;
	ToolServerSessionInfo::List  m_activeSessions;

	/// returns list of changed items pointers.
	std::vector<ToolServerInfo*> Update(const ToolServerInfo & newToolServer);
	/// returns list of changed items pointers.
	std::vector<ToolServerInfo*> Update(const std::deque<ToolServerInfo> &newNoolServers);

	std::string ToString(bool outputTools = false, bool outputClients = false) const;

	bool operator ==(const CoordinatorInfo& rh) const;
	bool operator !=(const CoordinatorInfo& rh) const { return !(*this == rh);}
};


}
