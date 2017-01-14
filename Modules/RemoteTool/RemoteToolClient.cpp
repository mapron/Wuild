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

#include "RemoteToolClient.h"

#include "RemoteToolFrames.h"

#include <CoordinatorClient.h>
#include <SocketFrameService.h>
#include <ThreadUtils.h>
#include <FileUtils.h>

#include <stdio.h>
#include <stdlib.h>
#include <functional>
#include <fstream>
#include <algorithm>

namespace Wuild
{
static const size_t g_recommendedBufferSize = 64 * 1024;

class ToolServerInfoWrap
{
public:
	SocketFrameHandler::Ptr m_handler;
	ToolServerInfo m_toolServerInfo;

	void UpdateBusy(int64_t mySessionId);
	bool m_state = false;
	int m_busyMine {0};
	int m_busyOthers {0};
	int m_eachTaskWeight = 32768;
};

class RemoteToolRequestWrap
{
public:
	RemoteToolRequest::Ptr m_request;
	SocketFrameHandler::ReplyNotifier m_callback;
	TimePoint m_expirationMoment;
	int m_attemptsRemain = 1;
};

class RemoteToolClientImpl
{
public:
	std::mutex m_clientsInfoMutex;
	std::deque<ToolServerInfoWrap> m_clientsInfo;
	std::mutex m_requestsMutex;
	std::deque<RemoteToolRequestWrap> m_requests;
	std::unique_ptr<SocketFrameService> m_server;
	CoordinatorClient m_coordinator;
	size_t m_clientIndex = 0;

	void QueueTask(const RemoteToolRequestWrap & task)
	{
		std::lock_guard<std::mutex> lock(m_requestsMutex);
		m_requests.push_back(task);
	}

	void ProcessTasks()
	{
		std::lock_guard<std::mutex> lock(m_requestsMutex);
		if (m_requests.empty())
			return;
		TimePoint now(true);
		for (auto it = m_requests.begin(); it != m_requests.end(); )
		{
			if (it->m_expirationMoment < now)
			{
				Syslogger(LOG_INFO) << "expired !!= " << SocketFrame::Ptr(it->m_request);
				if (it->m_callback)
					it->m_callback(nullptr, SocketFrameHandler::rsTimeout);
				it = m_requests.erase(it);
			}
			else
			{
				it++;
			}
		}

		if (m_requests.empty())
			return;

		std::lock_guard<std::mutex> lock2(m_clientsInfoMutex);

		RemoteToolRequestWrap & task = *m_requests.begin();

		SocketFrameHandler::Ptr handler;
		int minimalLoad = std::numeric_limits<int>::max();
		for (ToolServerInfoWrap & client : m_clientsInfo)
		{
			if (client.m_state)
			{
				const StringVector & toolIds = client.m_toolServerInfo.m_toolIds;
				const std::string & toolId = task.m_request->m_invocation.m_id.m_toolId;
				const bool toolExists = (std::find(toolIds.cbegin(), toolIds.cend(), toolId) != toolIds.cend());
				if (!toolExists) continue;
				int clientLoad = (client.m_busyMine + client.m_busyOthers) * client.m_eachTaskWeight / client.m_toolServerInfo.m_totalThreads;
				if (clientLoad < minimalLoad)
				{
					handler = client.m_handler;
					minimalLoad = clientLoad;
				}
			}
		}
		if (handler)
		{
			handler->QueueFrame(task.m_request, task.m_callback);
			m_requests.pop_front();
		}

	}
};


RemoteToolClient::RemoteToolClient(IInvocationRewriter::Ptr invocationRewriter)
	: m_impl(new RemoteToolClientImpl()), m_invocationRewriter(invocationRewriter)
{
	m_impl->m_coordinator.SetToolServerChangeCallback([this](const ToolServerInfo& info){
		{
			std::lock_guard<std::mutex> lock(m_impl->m_clientsInfoMutex);
			bool found = false;
			for (ToolServerInfoWrap & clientsInfo : m_impl->m_clientsInfo)
			{
				if (clientsInfo.m_toolServerInfo.EqualIdTo(info))
				{
					clientsInfo.m_toolServerInfo = info;
					clientsInfo.UpdateBusy(m_sessionId);
					found = true;
				}
			}
			if (!found)
			{
				AddClient(info, true);
			}
		}
		this->RecalcAvailable();
	});
}

RemoteToolClient::~RemoteToolClient()
{
	FinishSession();
	m_thread.Stop();

	for (auto & client : m_impl->m_clientsInfo)
		client.m_handler->Stop(false);
}

bool RemoteToolClient::SetConfig(const RemoteToolClient::Config &config)
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

int RemoteToolClient::GetFreeRemoteThreads() const
{
	return m_availableRemoteThreads - m_queuedTasks;
}

void RemoteToolClient::Start(const StringVector & requiredToolIds)
{
	m_started = true;
	m_start = TimePoint(true);
	m_sessionInfo = ToolServerSessionInfo();
	m_sessionInfo.m_sessionId = m_sessionId = m_start.GetUS();
	m_sessionInfo.m_clientId = m_config.m_clientId;
	m_requiredToolIds = requiredToolIds;
	for (auto & c : m_impl->m_clientsInfo)
		c.m_handler->Start();

	if (!m_impl->m_coordinator.SetConfig(m_config.m_coordinator))
		return;
	m_impl->m_coordinator.Start();

	m_thread.Exec(std::bind(&RemoteToolClientImpl::ProcessTasks, m_impl.get()));
}

void RemoteToolClient::FinishSession()
{
	if (!m_started)
		return;
	m_started = false;
	m_sessionInfo.m_elapsedTime = m_start.GetElapsedTime();
	m_impl->m_coordinator.SendToolServerSessionInfo(m_sessionInfo, true);
}

void RemoteToolClient::SetRemoteAvailableCallback(RemoteToolClient::RemoteAvailableCallback callback)
{
	m_remoteAvailableCallback = callback;
}

void RemoteToolClient::AddClient(const ToolServerInfo &info, bool start)
{
	if (!m_requiredToolIds.empty())
	{
		bool hasAtLeastOneTool = false;
		for (const auto & toolId : m_requiredToolIds)
		{
			if (std::find(info.m_toolIds.begin(), info.m_toolIds.end(), toolId) != info.m_toolIds.end())
			{
				hasAtLeastOneTool = true;
				break;
			}
		}
		if (!hasAtLeastOneTool)
		{
			Syslogger() << "Skipping client "<< info.m_connectionHost;
			return;
		}
	}
	Syslogger() << "RemoteToolClient::AddClient " << info.m_connectionHost  << ":" <<  info.m_connectionPort;
	ToolServerInfoWrap wrapNew;
	wrapNew.m_toolServerInfo = info;
	wrapNew.UpdateBusy(m_sessionId);
	m_impl->m_clientsInfo.push_back(std::move(wrapNew));
	ToolServerInfoWrap & wrap = *(m_impl->m_clientsInfo.rbegin());
	SocketFrameHandlerSettings settings;
	settings.m_channelProtocolVersion       = RemoteToolRequest::s_version + RemoteToolResponse::s_version;
	settings.m_recommendedRecieveBufferSize = g_recommendedBufferSize;
	settings.m_recommendedSendBufferSize    = g_recommendedBufferSize;
	wrap.m_handler.reset(new SocketFrameHandler( settings));
	wrap.m_handler->RegisterFrameReader(SocketFrameReaderTemplate<RemoteToolResponse>::Create());
	wrap.m_handler->SetTcpChannel(wrap.m_toolServerInfo.m_connectionHost, wrap.m_toolServerInfo.m_connectionPort);

	wrap.m_handler->SetChannelNotifier([this, &wrap](bool state){

		wrap.m_state = state;
		this->RecalcAvailable();
	});
	if (start)
	   wrap.m_handler->Start();
}

void RemoteToolClient::InvokeTool(const ToolInvocation & invocation, InvokeCallback callback)
{
	TimePoint start(true);
	const std::string inputFilename  = invocation.GetInput();
	const std::string outputFilename = invocation.GetOutput();
	auto taskIndex = m_taskIndex++;
	auto frameCallback = [this, callback, start, outputFilename, taskIndex](SocketFrame::Ptr responseFrame, SocketFrameHandler::TReplyState state)
	{
		m_queuedTasks--;
		Syslogger(LOG_INFO) << "RECIEVING [" << taskIndex << "]:" << outputFilename << ", m_queuedTasks=" << m_queuedTasks;
		TaskExecutionInfo info;
		if (state == SocketFrameHandler::rsTimeout)
		{
			info.m_stdOutput = "Timeout expired.";
		}
		else if (state == SocketFrameHandler::rsError)
		{
			info.m_stdOutput ="Internal error.";
		}
		else
		{
			RemoteToolResponse::Ptr result = std::dynamic_pointer_cast<RemoteToolResponse>(responseFrame);
			info.m_toolExecutionTime = result->m_executionTime;
			info.m_networkRequestTime = start.GetElapsedTime();

			info.m_result = result->m_result;
			info.m_stdOutput = result->m_stdOut;

			if (info.m_result && !outputFilename.empty())
			{
				info.m_result = FileInfo(outputFilename).WriteGzipped(result->m_fileData);
				if (!info.m_result)
					 Syslogger(LOG_ERR) << "Failed to write response data to " << outputFilename;
			}
		}
		UpdateSessionInfo(info);
		callback(info);
	};

	TaskExecutionInfo errInfo;
	errInfo.m_result = false;

	ByteArrayHolder inputData;
	if (!inputFilename.empty() && !FileInfo(inputFilename).ReadGzipped(inputData))
	{
		errInfo.m_stdOutput =  "failed to read " + inputFilename;
		callback(errInfo);
		return;
	}

	RemoteToolRequest::Ptr toolRequest(new RemoteToolRequest());
	toolRequest->m_invocation = m_invocationRewriter->PrepareRemote(invocation);
	toolRequest->m_fileData = inputData;
	toolRequest->m_sessionId = m_sessionId;
	RemoteToolRequestWrap wrap;
	wrap.m_request = toolRequest;
	wrap.m_callback = frameCallback;
	wrap.m_expirationMoment = TimePoint(true) + m_config.m_queueTimeout;
	wrap.m_attemptsRemain = m_config.m_invocationAttempts;

	{
		std::lock_guard<std::mutex> lock(m_sessionInfoMutex);
		m_sessionInfo.m_currentUsedThreads ++;
		Syslogger(LOG_INFO) << "QueueFrame [" << taskIndex << "] -> " << SocketFrame::Ptr(wrap.m_request) << " usedThreads: " << m_sessionInfo.m_currentUsedThreads;
	}
	m_queuedTasks++;
	m_impl->QueueTask(wrap);
}

void RemoteToolClient::RecalcAvailable()
{
	std::lock_guard<std::mutex> lock(m_impl->m_clientsInfoMutex);
	int available = 0;
	for (ToolServerInfoWrap & client : m_impl->m_clientsInfo)
	{
		if (client.m_state)
		{
			auto busyOthers=  client.m_busyOthers;
			if (busyOthers)
				busyOthers--;
			available += client.m_toolServerInfo.m_totalThreads - client.m_busyMine - busyOthers;
		}
	}
	Syslogger() << "RecalcAvailable:" << available;
	bool wasUnactive = m_availableRemoteThreads == 0;
	m_availableRemoteThreads = available;
	if (wasUnactive && available > 0 && m_remoteAvailableCallback)
	{
		m_remoteAvailableCallback(available);
	}
}

void RemoteToolClient::UpdateSessionInfo(const RemoteToolClient::TaskExecutionInfo &executionResult)
{
	std::lock_guard<std::mutex> lock(m_sessionInfoMutex);
	m_sessionInfo.m_tasksCount ++;
	if (!executionResult.m_result)
		m_sessionInfo.m_failuresCount++;
	 m_sessionInfo.m_totalNetworkTime += executionResult.m_networkRequestTime;
	 m_sessionInfo.m_totalExecutionTime += executionResult.m_toolExecutionTime;
	 m_sessionInfo.m_maxUsedThreads = std::max(m_sessionInfo.m_maxUsedThreads, m_sessionInfo.m_currentUsedThreads);
	 m_sessionInfo.m_currentUsedThreads--;

	m_impl->m_coordinator.SendToolServerSessionInfo(m_sessionInfo, false);
}

std::string RemoteToolClient::TaskExecutionInfo::GetProfilingStr() const
{
	std::ostringstream os;
	auto cus = m_toolExecutionTime.GetUS();
	auto nus = m_networkRequestTime.GetUS();
	auto overheadPercent = ((nus - cus) * 100) / (cus ? cus : 1);
	os << "compilationTime: " << cus << " us., "
	   << "networkTime: "  << nus << " us., "
	   << "overhead: " << overheadPercent << "%";
	return os.str();
}

void ToolServerInfoWrap::UpdateBusy(int64_t mySessionId)
{
	m_busyMine = 0;
	m_busyOthers = 0;
	for (const ToolServerInfo::ConnectedClientInfo & client : this->m_toolServerInfo.m_connectedClients)
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
