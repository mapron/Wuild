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

/// Retrives and send tool server worker information.
class CoordinatorClient
{
public:
	using WorkerChangeCallback = std::function<void(const CoordinatorWorkerInfo&)>;
	using InfoArrivedCallback = std::function<void(const CoordinatorInfo&, const WorkerSessionInfo::List &)>;
	using Config = CoordinatorClientConfig;

public:
	CoordinatorClient();
	~CoordinatorClient();

	bool SetConfig(const Config & config);
	void SetWorkerChangeCallback(WorkerChangeCallback callback);
	void SetInfoArrivedCallback(InfoArrivedCallback callback);

	void Start();

	void SetWorkerInfo(const CoordinatorWorkerInfo & info);
	void SendWorkerSessionInfo( const WorkerSessionInfo & sessionInfo );

protected:
	void Quant();
	WorkerChangeCallback m_workerChangeCallback;
	InfoArrivedCallback m_infoArrivedCallback;

	std::unique_ptr<SocketFrameHandler> m_client;

	TimePoint m_lastSend;
	ThreadLoop m_thread;
	std::atomic_bool m_clientState { false };

	CoordinatorInfo m_coord;
	CoordinatorWorkerInfo m_worker;
	std::atomic_bool m_needSendWorkerInfo {true};
	std::atomic_bool m_needRequestData {true};
	std::mutex m_coordMutex;
	std::mutex m_workerMutex;

	Config m_config;
};

}
