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

#include <CoordinatorTypes.h>

#include <mutex>
#include <atomic>

namespace Wuild
{
/**
 * Balancer used to split request between several tool servers.
 * Main goal: the request should gone to server with least load at this moment.
 *
 * To update client information, UpdateClient and SetClientActive is used.
 *
 * To recieve balancer most suitable client, call FindFreeClient.
 * StartTask and FinishTask updates load cache.
 * Get*Threads funcation used for overall statistics.
 */
class ToolBalancer
{
public:
	enum class ClientStatus { Added, Skipped, Updated };

public:
	ToolBalancer();
	~ToolBalancer();

	void SetRequiredTools(const StringVector & requiredToolIds);
	void SetSessionId(int64_t sessionId);

	ClientStatus UpdateClient(const ToolServerInfo & toolServer, size_t & index);
	void SetClientActive(size_t index, bool isActive);
	void SetServerSideLoad(size_t index, uint16_t load);

	size_t FindFreeClient(const std::string & toolId) const;
	void StartTask(size_t index);
	void FinishTask(size_t index);

	uint16_t GetTotalThreads() const { return m_totalRemoteThreads; }
	uint16_t GetFreeThreads() const { return m_freeRemoteThreads; }
	uint16_t GetUsedThreads() const { return m_usedThreads; }

	bool IsAllActive() const;

	/// Used for tests.
	std::vector<uint16_t> TestGetBusy() const;

protected:
	struct ClientInfo
	{
		ToolServerInfo m_toolServer;
		bool m_active = false;
		uint16_t m_serverSideQueue = 0;
		uint16_t m_serverSideQueuePrev = 0;
		uint16_t m_serverSideQueueAvg = 0;
		uint16_t m_busyMine = 0;
		uint16_t m_busyOthers = 0;
		uint16_t m_busyTotal = 0;
		uint16_t m_busyByNetworkLoad = 0;
		int64_t m_clientLoad = 0;
		int m_eachTaskWeight = 32768; //TODO: priority? configaration?
		void UpdateLoad(int64_t mySessionId);
	};

protected:
	void RecalcAvailable();

	std::atomic<uint16_t> m_totalRemoteThreads {0};
	std::atomic<uint16_t> m_freeRemoteThreads {0};
	std::atomic<uint16_t> m_usedThreads {0};

	int64_t m_sessionId = 0;

	std::deque<ClientInfo> m_clients;
	StringVector m_requiredToolIds;
	mutable std::mutex m_clientsMutex;
};

}
