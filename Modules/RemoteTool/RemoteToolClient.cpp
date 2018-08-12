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
#include "ToolBalancer.h"

#include <CoordinatorClient.h>
#include <SocketFrameService.h>
#include <ThreadUtils.h>
#include <FileUtils.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <fstream>
#include <algorithm>
#include <utility>

namespace Wuild
{
static const size_t g_recommendedBufferSize = 64 * 1024;


class RemoteToolRequestWrap
{
public:
	TimePoint m_start;
	int64_t m_taskIndex = 0;
	ToolInvocation m_invocation;
	std::string m_originalFilename;
	RemoteToolRequest::Ptr m_toolRequest;
	RemoteToolClient::InvokeCallback m_callback;
	TimePoint m_expirationMoment;
	TimePoint m_requestTimeout;
	int m_attemptsRemain = 1;
};

class RemoteToolClientImpl
{
public:
	RemoteToolClient *m_parent{}; // ugly..
	ToolBalancer m_balancer;
	std::mutex m_clientsMutex;
	std::deque<SocketFrameHandler::Ptr> m_clients;
	std::mutex m_requestsMutex;
	std::deque<RemoteToolRequestWrap> m_requests;
	std::unique_ptr<SocketFrameService> m_server;
	CoordinatorClient m_coordinator;
	size_t m_clientIndex = 0;
	std::atomic_int m_pendingTasks {0};

	void QueueTask(const RemoteToolRequestWrap & task)
	{
		std::lock_guard<std::mutex> lock(m_requestsMutex);
		m_requests.push_back(task);
		m_pendingTasks++;
	}

	void ProcessTasks()
	{
		RemoteToolRequestWrap task;
		{
			std::lock_guard<std::mutex> lock(m_requestsMutex);
			if (m_requests.empty())
				return;
			TimePoint now(true);
			for (auto it = m_requests.begin(); it != m_requests.end(); )
			{
				if (it->m_expirationMoment < now)
				{
					Syslogger(Syslogger::Err) << "Task expired: " << SocketFrame::Ptr(it->m_toolRequest)
											  << " expiration moment:" << it->m_expirationMoment.ToString() << ", now:" << now.ToString();
					if (it->m_callback)
					{
						RemoteToolClient::TaskExecutionInfo info;
						info.m_stdOutput = "Timeout expired.";
						it->m_callback(info);
					}
					it = m_requests.erase(it);
				}
				else
				{
					it++;
				}
			}

			if (m_requests.empty())
				return;

			task = *m_requests.begin();
		}

		size_t clientIndex = m_balancer.FindFreeClient(task.m_invocation.m_id.m_toolId);
		if (clientIndex == std::numeric_limits<size_t>::max())
			return;

		SocketFrameHandler::Ptr handler;
		{
			std::lock_guard<std::mutex> lock2(m_clientsMutex);
			handler = m_clients[clientIndex];
		}
		auto frameCallback = [this, task, clientIndex](SocketFrame::Ptr responseFrame, SocketFrameHandler::ReplyState state, const std::string & errorInfo)
		{
			m_balancer.FinishTask(clientIndex);
			const std::string outputFilename =  task.m_originalFilename;
			Syslogger(Syslogger::Info) << "RECIEVING [" << task.m_taskIndex << "]:" << outputFilename;
			RemoteToolClient::TaskExecutionInfo info;
			bool retry = false;
			if (state == SocketFrameHandler::ReplyState::Timeout)
			{
				info.m_stdOutput = "Timeout expired:" + outputFilename + ", start:" + task.m_start.ToString()
						+ " exp:" + task.m_expirationMoment.ToString() + ", remain:" + std::to_string(task.m_attemptsRemain)
						+ ", balancer.free:" + std::to_string(m_balancer.GetFreeThreads()) + ", extraInfo:" + errorInfo;
				retry = true;
			}
			else if (state == SocketFrameHandler::ReplyState::Error)
			{
				info.m_stdOutput = "Internal error. " + errorInfo;
				retry = true;
			}
			else
			{
				RemoteToolResponse::Ptr result = std::dynamic_pointer_cast<RemoteToolResponse>(responseFrame);
				info.m_toolExecutionTime = result->m_executionTime;
				info.m_networkRequestTime = task.m_start.GetElapsedTime();

				info.m_result = result->m_result;
				info.m_stdOutput = result->m_stdOut;
				std::replace(info.m_stdOutput.begin(), info.m_stdOutput.end(), '\r', ' ');

				if (info.m_result && !outputFilename.empty())
				{
					info.m_result = FileInfo(outputFilename).WriteCompressed(result->m_fileData, result->m_compression);
				}
			}
			m_parent->UpdateSessionInfo(info);
			if (task.m_attemptsRemain > 0 && retry)
			{
				Syslogger(Syslogger::Warning) << info.m_stdOutput << " Retrying (" << task.m_attemptsRemain << " attempts remain), args:" << task.m_invocation.GetArgsString(false);
				auto taskCopy = task;
				taskCopy.m_attemptsRemain--;
				taskCopy.m_taskIndex = this->m_parent->m_taskIndex++;
				taskCopy.m_expirationMoment = TimePoint(true) + m_parent->m_config.m_queueTimeout;
				this->QueueTask(taskCopy);
			}
			else
			{
				if (!this->m_parent->m_compilerVersionSuitable)
				{
					info.m_result = false;
					info.m_stdOutput = "Invalid compiler configurations. Search log for details.\n";
				}
				task.m_callback(info);
			}
		};
		m_balancer.StartTask(clientIndex);
		m_pendingTasks--;
		handler->QueueFrame(task.m_toolRequest, frameCallback, task.m_requestTimeout);
		{
			std::lock_guard<std::mutex> lock(m_requestsMutex);
			m_requests.pop_front();
		}
	}
};


RemoteToolClient::RemoteToolClient(IInvocationRewriter::Ptr invocationRewriter, const IVersionChecker::VersionMap & versionMap)
	: m_impl(new RemoteToolClientImpl())
	, m_invocationRewriter(std::move(invocationRewriter))
	, m_toolVersionMap(versionMap)
{
	m_impl->m_parent = this;
	m_impl->m_coordinator.SetInfoArrivedCallback([this](const CoordinatorInfo& info){
		for (const auto & client : info.m_toolServers)
			this->AddClient(client, true);
	});
}

RemoteToolClient::~RemoteToolClient()
{
	FinishSession();
	m_thread.Stop();

	for (auto & client : m_impl->m_clients)
		client->Stop();
}

bool RemoteToolClient::SetConfig(const RemoteToolClient::Config &config)
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

int RemoteToolClient::GetFreeRemoteThreads() const
{
	return static_cast<int>(m_impl->m_balancer.GetFreeThreads()) - m_impl->m_pendingTasks; // may be negative.
}

void RemoteToolClient::Start(const StringVector & requiredToolIds)
{
	m_started = true;
	m_start = m_lastFinish = TimePoint(true);
	m_sessionInfo = ToolServerSessionInfo();
	m_sessionInfo.m_sessionId = m_sessionId = m_start.GetUS();
	m_sessionInfo.m_clientId = m_config.m_clientId;
	m_impl->m_balancer.SetRequiredTools(requiredToolIds);
	m_impl->m_balancer.SetSessionId(m_sessionId);
	m_requiredToolIds = requiredToolIds;

	const auto & initialToolServers = m_config.m_initialToolServers;
	for (const auto & toolserverHost : initialToolServers.m_hosts)
	{
		ToolServerInfo info;
		info.m_connectionHost = toolserverHost;
		info.m_connectionPort = static_cast<int16_t>(initialToolServers.m_port);
		info.m_toolIds        = initialToolServers.m_toolIds;
		AddClient(info);
	}

	for (auto & handler : m_impl->m_clients)
		handler->Start();

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
	m_sessionInfo.m_elapsedTime = m_lastFinish - m_start;
	m_impl->m_coordinator.SendToolServerSessionInfo(m_sessionInfo, true);
}

void RemoteToolClient::SetRemoteAvailableCallback(RemoteToolClient::RemoteAvailableCallback callback)
{
	m_remoteAvailableCallback = std::move(callback);
}

void RemoteToolClient::AddClient(const ToolServerInfo &info, bool start)
{
	size_t index = 0;
	ToolBalancer & balancer = m_impl->m_balancer;
	auto status = balancer.UpdateClient(info, index);
	if (status == ToolBalancer::ClientStatus::Skipped)
		return;

	AvailableCheck();

	if (status == ToolBalancer::ClientStatus::Updated)
		return;

	Syslogger() << "RemoteToolClient::AddClient " << info.m_connectionHost  << ":" <<  info.m_connectionPort;

	SocketFrameHandlerSettings settings;
	settings.m_channelProtocolVersion       = RemoteToolRequest::s_version + RemoteToolResponse::s_version;
	settings.m_recommendedRecieveBufferSize = g_recommendedBufferSize;
	settings.m_recommendedSendBufferSize    = g_recommendedBufferSize;
	settings.m_segmentSize = 8192;
	settings.m_hasConnStatus = true;
	SocketFrameHandler::Ptr handler(new SocketFrameHandler( settings ));
	handler->RegisterFrameReader(SocketFrameReaderTemplate<RemoteToolResponse>::Create());
	handler->RegisterFrameReader(SocketFrameReaderTemplate<ToolsVersionResponse>::Create());
	handler->SetTcpChannel(info.m_connectionHost, info.m_connectionPort);

	handler->SetChannelNotifier([&balancer, index, this](bool state){
		balancer.SetClientActive(index, state);
		AvailableCheck();
	});
	handler->SetConnectionStatusNotifier([&balancer, index, this](SocketFrameHandler::ConnectionStatus status){
		balancer.SetServerSideLoad(index, status.uniqueRepliesQueued);
		AvailableCheck();
	});
	auto versionFrameCallback = [this, info](SocketFrame::Ptr responseFrame, SocketFrameHandler::ReplyState state, const std::string & errorInfo)
	{
		if (state == SocketFrameHandler::ReplyState::Timeout || state == SocketFrameHandler::ReplyState::Error)
		{
			Syslogger(Syslogger::Err) << "Error on requesting toolserver " << info.m_connectionHost << ", " << errorInfo;
		}
		else
		{
			ToolsVersionResponse::Ptr result = std::dynamic_pointer_cast<ToolsVersionResponse>(responseFrame);
			this->CheckRemoteToolVersions(result->m_versions, info.m_connectionHost);
		}
	};
	handler->QueueFrame(ToolsVersionRequest::Ptr(new ToolsVersionRequest()), versionFrameCallback, m_config.m_requestTimeout);

	{
		std::lock_guard<std::mutex> lock2(m_impl->m_clientsMutex);
		m_impl->m_clients.push_back(handler);
	}
	if (start)
	   handler->Start();
}

void RemoteToolClient::InvokeTool(const ToolInvocation & invocation, const InvokeCallback& callback)
{
	TimePoint start(true);
	const std::string inputFilename  = invocation.GetInput();
	ByteArrayHolder inputData;
	if (!inputFilename.empty() && !FileInfo(inputFilename).ReadCompressed(inputData, m_config.m_compression))
	{
		callback(RemoteToolClient::TaskExecutionInfo("failed to read " + inputFilename));
		return;
	}

	RemoteToolRequest::Ptr toolRequest(new RemoteToolRequest());
	toolRequest->m_invocation = m_invocationRewriter->PrepareRemote(invocation);
	toolRequest->m_fileData = inputData;
	toolRequest->m_compression = m_config.m_compression;
	toolRequest->m_sessionId = m_sessionId;
	toolRequest->m_clientId = m_config.m_clientId;

	RemoteToolRequestWrap wrap;
	wrap.m_start = start;
	wrap.m_toolRequest = toolRequest;
	wrap.m_taskIndex = m_taskIndex++;
	wrap.m_invocation = toolRequest->m_invocation;
	wrap.m_originalFilename = invocation.GetOutput();
	wrap.m_callback = callback;
	wrap.m_expirationMoment = TimePoint(true) + m_config.m_queueTimeout;
	wrap.m_attemptsRemain = m_config.m_invocationAttempts;
	wrap.m_requestTimeout = m_config.m_requestTimeout;

	Syslogger(Syslogger::Info) << "QueueFrame [" << wrap.m_taskIndex << "] -> " << toolRequest->m_invocation.m_id.m_toolId
							   << " " << toolRequest->m_invocation.GetArgsString(false)
						<< ", balancerFree:" <<m_impl->m_balancer.GetFreeThreads()
						<< ", pending:" << m_impl->m_pendingTasks;

	m_impl->QueueTask(wrap);
}


void RemoteToolClient::UpdateSessionInfo(const RemoteToolClient::TaskExecutionInfo &executionResult)
{
	std::lock_guard<std::mutex> lock(m_sessionInfoMutex);
	m_lastFinish = TimePoint(true);
	m_sessionInfo.m_tasksCount ++;
	if (!executionResult.m_result)
		m_sessionInfo.m_failuresCount++;
	m_sessionInfo.m_totalNetworkTime += executionResult.m_networkRequestTime;
	m_sessionInfo.m_totalExecutionTime += executionResult.m_toolExecutionTime;
	m_sessionInfo.m_currentUsedThreads = m_impl->m_balancer.GetUsedThreads();
	m_sessionInfo.m_maxUsedThreads = std::max(m_sessionInfo.m_maxUsedThreads, m_sessionInfo.m_currentUsedThreads);

	m_impl->m_coordinator.SendToolServerSessionInfo(m_sessionInfo, false);
}

void RemoteToolClient::AvailableCheck()
{
	std::lock_guard<std::mutex> lock(m_availableCheckMutex);
	if (!m_remoteIsAvailable && m_impl->m_balancer.IsAllActive() && m_impl->m_balancer.GetFreeThreads() > 0)
	{
		m_remoteIsAvailable = true;
		if (m_remoteAvailableCallback)
			m_remoteAvailableCallback();
		const auto free  = m_impl->m_balancer.GetFreeThreads();
		const auto total = m_impl->m_balancer.GetTotalThreads();
		Syslogger(Syslogger::Notice) << "Recieved info from coordinator: total remote threads=" << total << ", free=" << free;
	}
}

void RemoteToolClient::CheckRemoteToolVersions(const IVersionChecker::VersionMap &versionMap, const std::string & hostname)
{
	for (const auto & versionPair : versionMap)
	{
		const auto & toolId = versionPair.first;
		const auto & remoteVersion = versionPair.second;
		if (std::find(m_requiredToolIds.cbegin(), m_requiredToolIds.cend(), toolId) == m_requiredToolIds.cend())
			continue; // OK, we do not even need that tool.

		const auto localIt = m_toolVersionMap.find(toolId);
		if (localIt == m_toolVersionMap.end())
			continue; // OK, we do not have such a tool locally.

		const auto & localVersion = localIt->second;
		if (localVersion.empty() && remoteVersion.empty())
			continue; // OK, it's non-versioned tool

		if (localVersion == remoteVersion)
			continue; // OK, most common situation
		
		if (localVersion == InvocationRewriterConfig::VERSION_NO_CHECK || remoteVersion == InvocationRewriterConfig::VERSION_NO_CHECK)
			continue;

		Syslogger(Syslogger::Err) << "Tool id=" << toolId << " has local version='" << localVersion
								  << "' and remote version='" << remoteVersion << "' on '" << hostname  << "'";
		m_compilerVersionSuitable = false;
	}
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


}
