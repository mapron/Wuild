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

#include "CoordinatorClient.h"

#include <ThreadLoop.h>
#include <TimePoint.h>
#include <CommonTypes.h>
#include <RemoteToolClientConfig.h>
#include <ToolInvocation.h>

#include <functional>
#include <atomic>

namespace Wuild
{
class RemoteToolClientImpl;
class ToolServerInfoWrap;

/**
 * @brief Transforms local tool execution command to remote.
 *
 * Recieves remote tool servers list from Coordinator; then connects to all servers.
 * After reciving new task through InvokeTool() - distributes them to servers.
 */
class RemoteToolClient
{
public:
	/// Remote tool execution result.
	struct ExecutionInfo
	{
		TimePoint m_toolExecutionTime;
		TimePoint m_networkRequestTime;
		std::string GetProfilingStr() const;

		std::string m_stdOutput;
		bool m_result = false;
	};
	using Config = RemoteToolClientConfig;
	using RemoteAvailableCallback = std::function<void(int)>;
	using InvokeCallback = std::function<void(const ExecutionInfo& )>;

public:
	RemoteToolClient();
	~RemoteToolClient();

	bool SetConfig(const Config & config);

	/// Explicitly add new remote tool server client (used for testing)
	void AddClient(const ToolServerInfo & info, bool start = false);

	int GetFreeRemoteThreads() const;

	void Start();

	void SetRemoteAvailableCallback(RemoteAvailableCallback callback);

	/// Starts new remote task.
	void InvokeTool(const ToolInvocation & invocation, InvokeCallback callback);

protected:
	void AddClientInternal(ToolServerInfoWrap &info, bool start = false);
	void RecalcAvailable();

	ThreadLoop m_thread;

	std::unique_ptr<RemoteToolClientImpl> m_impl;
	std::atomic_int m_availableRemoteThreads {0};
	std::atomic_int m_queuedTasks {0};

	RemoteAvailableCallback m_remoteAvailableCallback;
	Config m_config;
};

}
