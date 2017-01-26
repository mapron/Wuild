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

#include "TcpListener.h"

#include "Tcp_private.h"
#include "TcpConnectionParams_private.h"
#include "TcpSocket.h"
#include "Syslogger.h"

namespace Wuild
{

class TcpListenerPrivate : public TcpSocketPrivate
{
};

TcpListener::TcpListener(const TcpConnectionParams & params)
	: m_impl(new TcpListenerPrivate())
	, m_params(params)
{
	SocketEngineCheck();
	m_logContext = m_params.GetShortInfo();
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

IDataListener::Ptr TcpListener::Create(const TcpConnectionParams &params)
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
		Syslogger(m_logContext, Syslogger::Err) << "Failed to listen on " << m_params.GetShortInfo() ;
		return false;
	}

	return IsListenerReadReady( );
}

bool TcpListener::StartListen()
{
	if (m_impl->m_socket != INVALID_SOCKET)
		return true;

	Syslogger(m_logContext, Syslogger::Info) << "Start listen on: " <<  m_params.GetShortInfo();
	if (!m_params.Resolve())
		return false;

	m_impl->m_socket = socket( m_params.m_impl->ai->ai_family, m_params.m_impl->ai->ai_socktype, m_params.m_impl->ai->ai_protocol );
	if (m_impl->m_socket == INVALID_SOCKET)
	{
		m_listenerFailed = true;
		return false;
	}

	m_impl->SetBlocking(false);

	int optval = 1;
	setsockopt(m_impl->m_socket, SOL_SOCKET, SO_REUSEADDR,
		   #ifdef _WIN32
			   (char*)
		   #endif
			   &optval, sizeof optval);

	int ret = ::bind( m_impl->m_socket, m_params.m_impl->ai->ai_addr, m_params.m_impl->ai->ai_addrlen );
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
	struct timeval timeout;
	SET_TIMEVAL_US(timeout, m_params.m_connectTimeout);
	int selected = select( m_impl->m_socket + 1, &set, 0, 0, &timeout );
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

	struct sockaddr_in incoming_address;
	socklen_t incoming_length = sizeof( incoming_address );
	Socket = accept( m_impl->m_socket, (struct sockaddr*)&incoming_address, &incoming_length );
	long value = 0;

	setsockopt( Socket, SOL_SOCKET, SO_KEEPALIVE, SOCK_OPT_ARG &value, sizeof(value) );

	// TODO: inet_ntop?
	const std::string peerIp = (incoming_length == sizeof(sockaddr_in) ? inet_ntoa( incoming_address.sin_addr ) : std::string());

	client->m_logContext = peerIp + "->:" + std::to_string(m_params.GetPort());

	client->m_impl->m_socket =  Socket;
	m_waitingAccept = false;
	const bool success = (Socket != INVALID_SOCKET);
	Syslogger() << "TcpListener::doAccept = " << success;
	if (!success)
		Syslogger(Syslogger::Err) << "Socket accept failed." ;

	return success;
}


}
