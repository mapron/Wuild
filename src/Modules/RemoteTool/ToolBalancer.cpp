/*
 * Copyright (C) 2017-2021 Smirnov Vladimir mapron1@gmail.com
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
#include <cassert>

namespace Wuild {

ToolBalancer::ToolBalancer() = default;

ToolBalancer::~ToolBalancer() = default;

void ToolBalancer::SetRequiredTools(const StringVector& requiredToolIds)
{
    m_requiredToolIds = requiredToolIds;
}

void ToolBalancer::SetSessionId(int64_t sessionId)
{
    m_sessionId = sessionId;
}

ToolBalancer::ClientStatus ToolBalancer::UpdateClient(const ToolServerInfo& toolServer, size_t& index)
{
    if (!m_requiredToolIds.empty() && !toolServer.m_toolIds.empty()) {
        bool hasAtLeastOneTool = false;
        for (const auto& toolId : m_requiredToolIds) {
            if (std::find(toolServer.m_toolIds.begin(), toolServer.m_toolIds.end(), toolId) != toolServer.m_toolIds.end()) {
                hasAtLeastOneTool = true;
                break;
            }
        }
        if (!hasAtLeastOneTool) {
            Syslogger() << "Skipping client " << toolServer.m_connectionHost;
            return ClientStatus::Skipped;
        }
    }

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    bool                        found = false;
    for (ClientInfo& clientsInfo : m_clients) {
        if (clientsInfo.m_toolServer.EqualIdTo(toolServer)) {
            clientsInfo.m_toolServer = toolServer;
            clientsInfo.UpdateLoad(m_sessionId);
            found = true;
        }
    }
    if (found) {
        RecalcAvailable();
        return ClientStatus::Updated;
    }

    ClientInfo clientInfo;
    clientInfo.m_toolServer = toolServer;
    clientInfo.UpdateLoad(m_sessionId);
    m_clients.push_back(clientInfo);
    index = m_clients.size() - 1;
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

void ToolBalancer::SetClientCompatible(size_t index, bool isCompatible)
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    assert(index < m_clients.size());
    m_clients[index].m_compatible = isCompatible;
    m_clients[index].m_checked    = true;
    RecalcAvailable();
}

void ToolBalancer::SetServerSideLoad(size_t index, uint16_t load)
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    ClientInfo&                 info = m_clients[index];
    info.m_serverSideQueuePrev       = info.m_serverSideQueue;
    info.m_serverSideQueue           = load;
    info.m_serverSideQueueAvg        = (info.m_serverSideQueue + info.m_serverSideQueuePrev) / 2;
    info.UpdateLoad(m_sessionId);
    RecalcAvailable();
}

size_t ToolBalancer::FindFreeClient(const std::string& toolId) const
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);

    int64_t minimalLoad = std::numeric_limits<int64_t>::max();
    size_t  freeIndex   = std::numeric_limits<size_t>::max();

    for (size_t index = 0; index < m_clients.size(); ++index) {
        const ClientInfo& client = m_clients[index];
        if (client.m_active && client.m_compatible) {
            const StringVector& toolIds    = client.m_toolServer.m_toolIds;
            const bool          toolExists = toolIds.empty() || (std::find(toolIds.cbegin(), toolIds.cend(), toolId) != toolIds.cend());
            if (!toolExists)
                continue;

            if (client.m_clientLoad < minimalLoad) {
                minimalLoad = client.m_clientLoad;
                freeIndex   = index;
            }
        }
    }

    return freeIndex;
}

void ToolBalancer::StartTask(size_t index)
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clients[index].m_busyMine++;
    m_clients[index].UpdateLoad(m_sessionId);
    RecalcAvailable();
}

void ToolBalancer::FinishTask(size_t index)
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    uint16_t&                   busyMine = m_clients[index].m_busyMine;
    if (busyMine)
        --busyMine;
    m_clients[index].UpdateLoad(m_sessionId);
    RecalcAvailable();
}

bool ToolBalancer::IsAllChecked() const
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (const ClientInfo& client : m_clients)
        if (!client.m_checked)
            return false;
    return true;
}

std::vector<uint16_t> ToolBalancer::TestGetBusy() const
{
    std::vector<uint16_t> result;
    for (const ClientInfo& client : m_clients)
        result.push_back(client.m_busyMine + client.m_busyOthers);
    return result;
}

void ToolBalancer::RecalcAvailable()
{
    uint16_t free = 0, used = 0, total = 0;
    for (const ClientInfo& client : m_clients) {
        if (client.m_active && client.m_compatible) {
            total += client.m_toolServer.m_totalThreads;
            free += client.m_toolServer.m_totalThreads - client.m_busyTotal;
            used += client.m_busyMine;
        }
    }
    m_totalRemoteThreads = total;
    m_freeRemoteThreads  = free;
    m_usedThreads        = used;
}

void ToolBalancer::ClientInfo::UpdateLoad(int64_t mySessionId)
{
    m_busyOthers = 0;
    for (const ToolServerInfo::ConnectedClientInfo& client : m_toolServer.m_connectedClients) {
        if (client.m_sessionId && client.m_sessionId != mySessionId)
            m_busyOthers += client.m_usedThreads;
    }
    if (m_busyOthers > 0)
        m_busyOthers--; // reduce other's load for more greedy behaviour.

    if (m_serverSideQueueAvg >= m_busyMine - 1) {
        // if server has large queue, lower free threads.
        m_busyByNetworkLoad++;
        m_busyByNetworkLoad = std::min(m_busyByNetworkLoad, m_toolServer.m_totalThreads);
    }
    if (m_serverSideQueueAvg == 0 && m_busyByNetworkLoad) {
        // if server has empty queue, remove penalty
        m_busyByNetworkLoad--;
    }

    m_busyTotal = m_busyOthers + m_busyMine + m_busyByNetworkLoad;
    m_busyTotal = std::min(m_busyTotal, m_toolServer.m_totalThreads);

    m_clientLoad = m_toolServer.m_totalThreads ? (m_busyTotal * m_eachTaskWeight / m_toolServer.m_totalThreads) : 0;
}

}
