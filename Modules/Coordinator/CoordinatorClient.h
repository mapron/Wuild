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

#include "CoordinatorTypes.h"

#include <ThreadLoop.h>
#include <CoordinatorClientConfig.h>

#include <functional>
#include <atomic>
#include <mutex>

namespace Wuild
{
class SocketFrameHandler;

/// Retrives and send tool server information.
class CoordinatorClient
{
public:
	using ToolServerChangeCallback = std::function<void(const ToolServerInfo&)>;
	using InfoArrivedCallback = std::function<void(const CoordinatorInfo&, const ToolServerSessionInfo::List &)>;
	using Config = CoordinatorClientConfig;

public:
	CoordinatorClient();
	~CoordinatorClient();

	bool SetConfig(const Config & config);
	void SetToolServerChangeCallback(ToolServerChangeCallback callback);
	void SetInfoArrivedCallback(InfoArrivedCallback callback);

	void Start();

	void SetToolServerInfo(const ToolServerInfo & info);
	void SendToolServerSessionInfo( const ToolServerSessionInfo & sessionInfo );

protected:
	void Quant();
	ToolServerChangeCallback m_toolServerChangeCallback;
	InfoArrivedCallback m_infoArrivedCallback;

	std::unique_ptr<SocketFrameHandler> m_client;

	TimePoint m_lastSend;
	ThreadLoop m_thread;
	std::atomic_bool m_clientState { false };

	CoordinatorInfo m_coord;
	ToolServerInfo m_toolServerInfo;
	std::atomic_bool m_needSendToolServerInfo {true};
	std::atomic_bool m_needRequestData {true};
	std::mutex m_coordMutex;
	std::mutex m_toolServerInfoMutex;

	Config m_config;
};

}
