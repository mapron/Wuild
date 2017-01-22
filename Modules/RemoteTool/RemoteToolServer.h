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

#include "ILocalExecutor.h"
#include "CoordinatorClient.h"

#include <RemoteToolServerConfig.h>

#include <functional>
#include <atomic>

namespace Wuild
{
class RemoteToolServerImpl;
/// Listening port for incoming tool execution tasks and transforms it to LocalExecutor.
class RemoteToolServer
{
public:
	using Config = RemoteToolServerConfig;

public:
	RemoteToolServer(ILocalExecutor::Ptr executor);
	~RemoteToolServer();

	bool SetConfig(const Config & config);

	void Start();

protected:
	void StartTask(const std::string & clientId, int64_t sessionId);
	void FinishTask(int64_t sessionId, bool remove);
	void UpdateInfo();
	std::atomic<uint16_t> m_runningTasks {0};
	Config m_config;
	std::unique_ptr<RemoteToolServerImpl> m_impl;

};

}
