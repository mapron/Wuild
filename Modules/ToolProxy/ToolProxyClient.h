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
#include <ThreadLoop.h>
#include <ToolProxyServerConfig.h>

#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace Wuild
{
class SocketFrameHandler;
/**
 * @brief Local compiler emulator
 *
 * Translates local invocation to network request to tool rpoxy server.
 * When request is done, outputs result of tool invocation to stderr.
 */
class ToolProxyClient
{
public:
	using Config = ToolProxyServerConfig;

public:
	ToolProxyClient();
	~ToolProxyClient();

	bool SetConfig(const Config & config);

	bool Start(TimePoint connectionTimeout = TimePoint(1.0));

	/// Invoke local compile task. It's not splitted.
	void RunTask(const StringVector & args);

protected:
	std::unique_ptr<SocketFrameHandler> m_client;
	Config m_config;
	std::atomic_bool m_connectionState {false};
	std::condition_variable m_connectionStateCond;
	std::mutex m_connectionStateMutex;

};

}
