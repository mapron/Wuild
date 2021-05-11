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
#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include "TcpListener.h"
#include "Tcp_private.h"
#include "TcpConnectionParams_private.h"
#include "TcpSocket.h"
#include "Syslogger.h"

#include <utility>

namespace Wuild
{

class TcpListenerPrivate : public TcpSocketPrivate
{
};

TcpListener::TcpListener(TcpListenerParams  params)
	: m_impl(new TcpListenerPrivate())
	, m_params(std::move(params))
{
	SocketEngineCheck();
	m_logContext = m_params.m_endPoint.GetShortInfo();
	Syslogger(m_logContext) << "Adding TCP server";
}

TcpListener::~TcpListener()
{
	if (m_impl->m_socket != INVALID_SOCKET)
	{
		Syslogger(m_logContext) << "disconnecting listener..." ;
		close( m_impl->m_socket );
	}
}

IDataListener::Ptr TcpListener::Create(const TcpListenerParams &params)
{
	return IDataListener::Ptr(new TcpListener(params));
}

IDataSocket::Ptr TcpListener::GetPendingConnection()
{
	if (m_waitingAccept)
		return nullptr;
	if (!HasPendingConnections())
		return nullptr;
	if (m_waitingAccept)
		return nullptr;
	m_waitingAccept = true;
	Syslogger(m_logContext) << "new connection established" ;
	return TcpSocket::Create(m_params, this);
}

bool TcpListener::HasPendingConnections()
{
	if (m_listenerFailed && m_params.m_skipFailedConnection)
		return false;

	if (!StartListen())
	{
		if (m_params.m_connectionFailureCallback)
			m_params.m_connectionFailureCallback();
		Syslogger(m_logContext, Syslogger::Err) << "Failed to listen on " << m_params.m_endPoint.GetShortInfo() ;
		return false;
	}

	return IsListenerReadReady( );
}

bool TcpListener::StartListen()
{
	if (m_impl->m_socket != INVALID_SOCKET)
		return true;

	Syslogger(m_logContext, Syslogger::Info) << "Start listen on: " <<  m_params.m_endPoint.GetShortInfo();
	if (!m_params.m_endPoint.Resolve() || !m_params.Resolve())
		return false;

	m_impl->m_socket = m_params.m_endPoint.GetImpl().MakeSocket();
	if (m_impl->m_socket == INVALID_SOCKET)
	{
		m_listenerFailed = true;
		return false;
	}

	m_impl->SetBlocking(false);

	int optval = 1;
	setsockopt(m_impl->m_socket, SOL_SOCKET,  m_params.m_reuseSockets ? SO_REUSEADDR : 0,
		   #ifdef _WIN32
			   (char*)
		   #endif
			   &optval, sizeof optval);

	int ret = m_params.m_endPoint.GetImpl().Bind(m_impl->m_socket);
	if (ret < 0)
	{
		close( m_impl->m_socket );
		m_impl->m_socket = INVALID_SOCKET;
		m_listenerFailed = true;
		return false;
	}

	if (listen(m_impl->m_socket, m_params.m_pendingListenConnections))
	{
		close( m_impl->m_socket );
		m_impl->m_socket = INVALID_SOCKET;
		m_listenerFailed = true;
		return false;
	}

	return true;
}


bool TcpListener::IsListenerReadReady()
{
	fd_set set;
	FD_ZERO( &set );
	FD_SET( m_impl->m_socket, &set );

	//select() modifies timeout.
	struct timeval timeout{};
	SET_TIMEVAL_US(timeout, m_params.m_connectTimeout);
	int selected = select( static_cast<int>(m_impl->m_socket + 1), &set, nullptr, nullptr, &timeout );
	if (selected < 0)
	{
		Syslogger(m_logContext) << "Disconnect from IsListenerReadReady" ;
		return false;
	}
	bool res = (selected != 0 && FD_ISSET( m_impl->m_socket, &set ));
	return res;
}


bool TcpListener::DoAccept(TcpSocket *client)
{
	SOCKET Socket = INVALID_SOCKET;

	struct sockaddr_in incoming_address{};
	socklen_t incoming_length = sizeof( incoming_address );
	Socket = accept( m_impl->m_socket, (struct sockaddr*)&incoming_address, &incoming_length );
	long value = 0;

	setsockopt( Socket, SOL_SOCKET, SO_KEEPALIVE, SOCK_OPT_ARG &value, sizeof(value) );

	// TODO: inet_ntop?
	const std::string peerIp = (incoming_length == sizeof(sockaddr_in) ?  TcpEndPointPrivate::AddrToString( &incoming_address.sin_addr ) : std::string());
	std::string err;
	if (!m_params.IsAccepted(peerIp, err))
	{
		if (Socket != INVALID_SOCKET)
			close(Socket);
		Syslogger(Syslogger::Err) << "Socket accept failed. Got " << peerIp << ", but only these allowed:" << err ;
		return false;
	}
	client->m_logContext = peerIp + "->:" + std::to_string(m_params.m_endPoint.GetPort());

	client->m_impl->m_socket =  Socket;
	m_waitingAccept = false;
	const bool success = (Socket != INVALID_SOCKET);
	Syslogger() << "TcpListener::doAccept = " << success;
	if (!success)
		Syslogger(Syslogger::Err) << "Socket accept failed." ;

	return success;
}


}
