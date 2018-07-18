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

#include "SocketFrameService.h"

#include "TcpListener.h"
#include "ThreadUtils.h"

#include <functional>
#include <utility>

namespace Wuild
{

SocketFrameService::SocketFrameService(const SocketFrameHandlerSettings & settings, int autoStartListenPort, const StringVector & whiteList)
	: m_settings(settings)
{
	if (autoStartListenPort > 0)
		AddTcpListener(autoStartListenPort, "*", whiteList);
}

SocketFrameService::~SocketFrameService()
{
	Syslogger(m_logContext) << "SocketFrameService::~SocketFrameService()";
	Stop(); ///< @warning stop thread before any deinitialization
	for (const auto& workerPtr : m_workers)
	{
		workerPtr->Stop(); ///< @warning stop thread before any deinitialization
		if (m_handlerDestroyCallback)
			m_handlerDestroyCallback(workerPtr.get());
	}
	m_workers.clear();
	m_listenters.clear();
}

int SocketFrameService::QueueFrameToAll(SocketFrameHandler *sender, const SocketFrame::Ptr& message)
{
	Syslogger(m_logContext) << "QueueFrameToAll";
	int ret = 0;
	std::lock_guard<std::mutex> lock(m_workersLock);

	for (const auto & workerPtr : m_workers)
	{
		if (workerPtr->IsActive())
		{
			if (workerPtr.get() == sender) continue;
			workerPtr->QueueFrame(message);
			++ret;
		}
	}
	return ret;
}

void SocketFrameService::AddTcpListener(int port, const std::string & host, const StringVector & whiteList)
{
	TcpListenerParams params;
	params.m_endPoint.SetPoint(port, host);
	params.m_connectTimeout = TimePoint(0.001);
	params.m_recommendedRecieveBufferSize = m_settings.m_recommendedRecieveBufferSize;
	params.m_recommendedSendBufferSize    = m_settings.m_recommendedSendBufferSize;
	for (const auto & host : whiteList)
		params.AddWhiteListPoint(port, host);

	auto listener = TcpListener::Create(params);
	if (!m_logContext.empty())
		m_logContext += ", ";
	m_logContext += listener->GetLogContext();
	m_listenters.push_back(listener);
}

void SocketFrameService::Start()
{
	m_mainThread.Exec(std::bind(&SocketFrameService::Quant, this), m_settings.m_mainThreadSleep.GetUS());
}

void SocketFrameService::Stop()
{
	m_mainThread.Stop();
}

void SocketFrameService::Quant()
{
	std::lock_guard<std::mutex> lock(m_workersLock);

	// first, check for dead workers and erase them
	auto workerIt = m_workers.begin();
	while (workerIt != m_workers.end())
	{
		if (!(*workerIt)->IsActive())
		{
			(*workerIt)->Stop(); ///< @warning stop thread before any deinitialization
			if (m_handlerDestroyCallback)
				m_handlerDestroyCallback((*workerIt).get());

			Syslogger(m_logContext) << "SocketFrameService::Quant() erasing unactive worker " << (*workerIt)->GetThreadId();
			workerIt = m_workers.erase(workerIt);
			continue;
		}
		++workerIt;
	}

	// second, proceed all pending connections.
	for (IDataListener::Ptr & listenter : m_listenters)
	{
		auto client = listenter->GetPendingConnection(); //TODO: while?
		if (client)
		{
			const int newId = m_nextWorkerId++;
			Syslogger(m_logContext) << "SocketFrameService::quant() adding new worker " << newId;
			AddWorker(client, newId);
		}
	}
}

void SocketFrameService::AddWorker(IDataSocket::Ptr client, int threadId)
{
	std::shared_ptr<SocketFrameHandler> handler(new SocketFrameHandler(threadId, m_settings));
	handler->SetRetryConnectOnFail(false);
	for (const auto & reader : m_readers)
		handler->RegisterFrameReader(reader);

	if (m_handlerInitCallback)
		m_handlerInitCallback(handler.get());

	if (threadId > -1)
		handler->SetChannel(std::move(client));

	handler->Start();
	m_workers.push_back(handler);
}

bool SocketFrameService::IsConnected()
{
	return GetActiveConnectionsCount() > 0;
}

size_t SocketFrameService::GetActiveConnectionsCount()
{
	if (m_workers.empty())
		return 0;

	std::lock_guard<std::mutex> lock(m_workersLock);
	size_t res = 0;
	for (const auto& workerPtr : m_workers)
		if (workerPtr->IsActive())
			res++;
	return res;
}

void SocketFrameService::RegisterFrameReader(const SocketFrameHandler::IFrameReader::Ptr& reader)
{
	m_readers.push_back(reader);
}

void SocketFrameService::SetHandlerInitCallback(SocketFrameService::HandlerInitCallback callback)
{
	m_handlerInitCallback = std::move(callback);
}

void SocketFrameService::SetHandlerDestroyCallback(HandlerDestroyCallback callback)
{
	m_handlerDestroyCallback = std::move(callback);
}

}
