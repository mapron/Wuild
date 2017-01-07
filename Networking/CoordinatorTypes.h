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
struct CoordinatorWorkerInfo
{
	std::string m_workerId;
	std::string m_connectionHost;
	int16_t m_connectionPort = 0;
	StringVector m_toolIds;
	uint16_t m_totalThreads = 0;

	struct ConnectedClientInfo
	{
		uint16_t m_usedThreads = 0;
		std::string m_clientHost;
		std::string m_clientId;
		int64_t m_sessionId = 0;

		bool operator ==(const ConnectedClientInfo& rh) const;
		bool operator !=(const ConnectedClientInfo& rh) const { return !(*this == rh);}
	};
	std::vector<ConnectedClientInfo> m_connectedClients;
	std::string ToString(bool outputTools = false, bool outputClients = false) const;
	bool EqualIdTo(const CoordinatorWorkerInfo & rh) const;

	void *m_opaqueFrameHandler = nullptr;

	bool operator ==(const CoordinatorWorkerInfo& rh) const;
	bool operator !=(const CoordinatorWorkerInfo& rh) const { return !(*this == rh);}
};

/// Worker info list
struct CoordinatorInfo
{
	std::deque<CoordinatorWorkerInfo> m_workers;

	/// returns list of changed items pointers.
	std::vector<CoordinatorWorkerInfo*> Update(const CoordinatorWorkerInfo & newWorker);
	/// returns list of changed items pointers.
	std::vector<CoordinatorWorkerInfo*> Update(const std::deque<CoordinatorWorkerInfo> &newWorkers);

	std::string ToString(bool outputTools = false, bool outputClients = false) const;

	bool operator ==(const CoordinatorInfo& rh) const;
	bool operator !=(const CoordinatorInfo& rh) const { return !(*this == rh);}
};

/// Information about finished compilation session (sequence of tool executions)
struct WorkerSessionInfo
{
	using List = std::deque<WorkerSessionInfo>;
	std::string m_clientId;
	uint64_t m_sessionId;
	TimePoint m_totalExecutionTime;
	uint32_t m_tasksCount = 0;
	uint32_t m_failuresCount = 0;

	std::string ToString(bool full = false) const;
};

}
