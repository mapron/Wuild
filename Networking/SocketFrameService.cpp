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

#include <TcpListener.h>
#include <Application.h>
#include <ThreadUtils.h>

#include <functional>

namespace Wuild
{

SocketFrameService::SocketFrameService(const SocketFrameHandlerSettings & settings, int autoStartListenPort)
	: m_settings(settings)
{
	if (autoStartListenPort > 0)
		AddTcpListener(autoStartListenPort);
}

SocketFrameService::SocketFrameService(int autoStartListenPort)
	: m_settings{}
{
	if (autoStartListenPort > 0)
		AddTcpListener(autoStartListenPort);
}

SocketFrameService::~SocketFrameService()
{
	Syslogger() << "SocketFrameService::~SocketFrameService()";
	for (auto workerPtr : m_workers)
		workerPtr->Stop(false);
	m_workers.clear();
	m_listenters.clear();
	Stop();
}

int SocketFrameService::QueueFrameToAll(SocketFrameHandler *sender, SocketFrame::Ptr message)
{
	Syslogger() << "QueueFrameToAll";
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

void SocketFrameService::AddTcpListener(int port, std::string ip)
{
	TcpConnectionParams params;
	if (!params.SetPoint(port, ip))
		return;

	params.m_connectTimeout = TimePoint(0.001);
	params.m_recommendedRecieveBufferSize = m_settings.m_recommendedRecieveBufferSize;
	m_listenters.push_back(TcpListener::Create(params));
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
	auto i = m_workers.begin();
	while (i != m_workers.end())
	{
		if (!(*i)->IsActive())
		{
			if (m_handlerDestroyCallback)
				m_handlerDestroyCallback((*i).get());

			Syslogger() << "SocketFrameService::quant() erasing unactive worker ";
			i = m_workers.erase(i);
			return;
		}
		++i;
	}
	for (IDataListener::Ptr & listenter : m_listenters)
	{
		auto client = listenter->GetPendingConnection();
		if (client)
		{
			Syslogger() << "SocketFrameService::quant() adding new worker ";
			AddWorker(client, m_nextWorkerId++);
		}
	}
}

void SocketFrameService::AddWorker(IDataSocket::Ptr client, int threadId)
{
	std::shared_ptr<SocketFrameHandler> handler(new SocketFrameHandler(m_settings));
	handler->SetRetryConnectOnFail(false);
	for (const auto & reader : m_readers)
		handler->RegisterFrameReader(reader);

	if (m_handlerInitCallback)
		m_handlerInitCallback(handler.get());

	if (threadId > -1)
		handler->SetChannel(client);

	handler->Start();
	m_workers.push_back(handler);
}

bool SocketFrameService::IsConnected()
{
	return GetActiveConnectionsCount() > 0;
}

size_t SocketFrameService::GetActiveConnectionsCount()
{
	if (!m_workers.size())
		return 0;

	std::lock_guard<std::mutex> lock(m_workersLock);
	size_t res = 0;
	for (auto workerPtr : m_workers)
		if (workerPtr->IsActive())
			res++;
	return res;
}

void SocketFrameService::RegisterFrameReader(SocketFrameHandler::IFrameReader::Ptr reader)
{
	m_readers.push_back(reader);
}

void SocketFrameService::SetHandlerInitCallback(SocketFrameService::HandlerInitCallback callback)
{
	m_handlerInitCallback = callback;
}

void SocketFrameService::SetHandlerDestroyCallback(HandlerDestroyCallback callback)
{
	m_handlerDestroyCallback = callback;
}

}
