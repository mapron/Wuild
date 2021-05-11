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

#include "TimePoint.h"
#include "IDataSocket.h"
#include "TcpConnectionParams.h"

#include <map>
#include <string>
#include <atomic>

namespace Wuild
{

class TcpListener;
class TcpSocketPrivate;
/// Implementation of TCP data socket.
/// Can be created by own or through TcpListener::GetPendingConnection.
class TcpSocket : public IDataSocket
{
	friend class TcpListener;
public:
	enum class ConnectionState { None , Pending , Success, Fail };
	static std::atomic_bool s_applicationInterruption;

public:
	TcpSocket (const TcpConnectionParams & params);
	virtual ~TcpSocket ();

	/// Creates new sockert. If pendingListener is set, then socket will be listener client.
	static IDataSocket::Ptr Create(const TcpConnectionParams & params, TcpListener* pendingListener = nullptr);

	bool Connect () override;
	void Disconnect () override;
	bool IsConnected () const override;
	bool IsPending() const override;

	ReadState Read(ByteArrayHolder & buffer) override;
	WriteState Write(const ByteArrayHolder & buffer, size_t maxBytes) override;

	/// Socker buffer size available for reading.
	uint32_t GetRecieveBufferSize() const override { return m_recieveBufferSize; }
	uint32_t GetSendBufferSize() const override { return m_sendBufferSize; }

	std::string GetLogContext() const override { return m_logContext; }

protected:
	void SetListener(TcpListener* pendingListener);
	void Fail ();     //!< Ошибка при установлении соединения.
	bool IsSocketReadReady ();
	bool SelectRead (const TimePoint & timeout);
	void SetBufferSize();

	TcpListener*         m_pendingListener = nullptr;
	ConnectionState      m_state = ConnectionState::None;
	bool                 m_acceptedByListener = false;
	TcpConnectionParams  m_params;
	uint32_t             m_recieveBufferSize = 0;
	uint32_t             m_sendBufferSize = 0;
	std::string          m_logContext;

private:
	std::unique_ptr<TcpSocketPrivate> m_impl;
};

}
