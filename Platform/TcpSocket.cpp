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

#include "TcpSocket.h"

#include "Tcp_private.h"
#include "TcpConnectionParams_private.h"
#include "TcpListener.h"
#include "Syslogger.h"

#include <algorithm>
#include <vector>
#include <stdint.h>
#include <memory>
#include <cstring>

//#define SOCKET_DEBUG  // for debugging.

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifdef TCP_SOCKET_WIN

void SocketEngineCheck()
{
	struct WSA_RAII {
		WSA_RAII() {  int ec;
					 WSADATA wsa;

							  if ((ec = WSAStartup(MAKEWORD(2,0), &wsa)) != 0)
								  Wuild::Syslogger(LOG_ERR) <<  "WIN32_SOCKET: winsock error: code " << ec;
				  }
		~WSA_RAII() { WSACleanup(); }
	};

	static WSA_RAII init ;
}

#endif

namespace
{
	const size_t g_defaultBufferSize = 4 * 1024;
}

namespace Wuild
{

std::atomic_bool TcpSocket::s_applicationInterruption(false);

TcpSocket::TcpSocket(const TcpConnectionParams &params)
	: m_params(params)
	, m_impl(new TcpSocketPrivate())
{
	SocketEngineCheck();
	m_logContext = params.GetShortInfo();
	Syslogger(m_logContext) << "TcpSocket::TcpSocket() ";
}

TcpSocket::~TcpSocket()
{
	Disconnect();
}

IDataSocket::Ptr TcpSocket::Create(const TcpConnectionParams &params, TcpListener *pendingListener)
{
	auto sock  = new TcpSocket(params);
	sock->SetListener(pendingListener);
	return IDataSocket::Ptr(sock);
}

bool TcpSocket::Connect ()
{
	if (s_applicationInterruption)
		return false;

	if (m_state == csSuccess)
		return true;

	if (m_acceptedByListener)
	{
		if (m_state == csFail)
			return false;

		if (m_state == csPending && m_pendingListener->DoAccept(this))
		{
			SetBufferSize();
			if (m_impl->SetBlocking(false))
			{
				m_state = csSuccess;
				return true;
			}
		}
		m_state = csFail;
		return false;
	}
	Syslogger(m_logContext) << "Trying to connect..." ;

	if (m_impl->m_socket == INVALID_SOCKET)
	{
		m_impl->m_socket = socket(m_params.m_impl->ai->ai_family, m_params.m_impl->ai->ai_socktype, m_params.m_impl->ai->ai_protocol);
		if (m_impl->m_socket == INVALID_SOCKET)
		{
			Syslogger(m_logContext, LOG_ERR) << "socket creation failed." ;
			Fail();
			return false;
		}
	}
	SetBufferSize();
	if (!m_impl->SetBlocking(false))
	{
		Fail();
		return false;
	}

	int cres = connect(m_impl->m_socket, m_params.m_impl->ai->ai_addr, m_params.m_impl->ai->ai_addrlen);
	if (cres < 0)
	{
#ifdef _WIN32
		const auto err = GetLastError();
		const bool inProgress = err == WSAEWOULDBLOCK;
#else
		const auto err = errno ;
		const bool inProgress = err == EINPROGRESS;
#endif
		if (inProgress)
		{
			struct timeval timeout;
			SET_TIMEVAL_US(timeout, m_params.m_connectTimeout);
			fd_set select_set;
			FD_ZERO( &select_set );
			FD_SET( m_impl->m_socket, &select_set );
			int valopt;
			socklen_t valopt_len = sizeof(valopt);

			if (select( m_impl->m_socket + 1, 0, &select_set, 0, &timeout ) <= 0 || !FD_ISSET( m_impl->m_socket, &select_set ) ||
				getsockopt( m_impl->m_socket, SOL_SOCKET, SO_ERROR, SOCK_OPT_ARG (&valopt), &valopt_len ) < 0 || valopt
					)
			{
				Syslogger(m_logContext) << "Connection timeout." ;
				Fail();
				return false;
			}
		}
		else
		{
			Syslogger(m_logContext) << "Connection failed. ("<< cres << ") err=" << err ;
			Fail();
			return false;
		}
	}

	m_state = csSuccess;
	Syslogger(m_logContext) << "Connected.";
	return true;
}

void TcpSocket::Disconnect()
{
	m_state = csFail;
	if (m_impl->m_socket != INVALID_SOCKET)
	{
		Syslogger(m_logContext) << "TcpSocket::Disconnect() ";
		close( m_impl->m_socket );
		m_impl->m_socket = INVALID_SOCKET;
	}
}

bool TcpSocket::IsConnected() const
{
	return m_state == csSuccess;
}

bool TcpSocket::IsPending() const
{
	return m_state == csPending;
}

bool TcpSocket::Read(ByteArrayHolder & buffer)
{
	if (!IsSocketReadReady( ))
		return false;

	if (!SelectRead( m_params.m_readTimeout ))  //Нет данных в порту
		return false;

	size_t bufferInitialSize = buffer.size();(void)bufferInitialSize;
	char tmpbuffer[1024];
	int recieved, totalRecieved = 0;
	do {

	  recieved =
		#ifdef TCP_SOCKET_WIN
			recv( m_impl->m_socket, tmpbuffer, sizeof(tmpbuffer), 0 );
		#else
			read( m_impl->m_socket, tmpbuffer, sizeof(tmpbuffer) );
		#endif
	  if (recieved == 0 && totalRecieved > 0)
		  break;

	  if (recieved <= 0)
	  {
#ifdef _WIN32
		  const auto err = GetLastError();
#else
		  const auto err = errno ;
		  if (err == EAGAIN)
			  break;
#endif
		  Syslogger(m_logContext) << "Disconnecting while Reading" << err ;
		  Disconnect();
		  return false;
	  }
	  totalRecieved += recieved;
	  buffer.ref().insert(buffer.ref().end(), tmpbuffer, tmpbuffer + recieved );

	} while(recieved == sizeof(tmpbuffer));

#ifdef SOCKET_DEBUG
	Syslogger(m_logContext) << "AbstractChannel::Read: " << Syslogger::Binary(buffer.data() + bufferInitialSize, totalRecieved);
#endif

	return true;
}

bool TcpSocket::Write(const ByteArrayHolder & buffer, size_t maxBytes)
{
	if (m_impl->m_socket == INVALID_SOCKET)
		return false;

	maxBytes = std::min(maxBytes, buffer.size());

	auto written = send( m_impl->m_socket, (const char*)(buffer.data()), maxBytes, MSG_NOSIGNAL);
	if (written < 0)
	{
#ifdef _WIN32
		const auto err = GetLastError();
#else
		const auto err = errno ;
#endif
		Syslogger(m_logContext) << "Disconnecting while Writing, err=" << err ;
		Disconnect();
		return false;
	}

#ifdef SOCKET_DEBUG
	Syslogger(m_logContext) << "AbstractChannel::Write: " << Syslogger::Binary(buffer.data(), written);
#endif
	return maxBytes == written;
}

void TcpSocket::SetListener(TcpListener* pendingListener)
{
	if (!pendingListener)
		return;

	m_state = csPending;
	m_pendingListener = pendingListener;
	m_acceptedByListener = true;
}

void TcpSocket::Fail ()
{
	Syslogger(m_logContext) << "Disconnection cased by Fail()";
	Disconnect();
}

bool TcpSocket::IsSocketReadReady()
{
	if (m_impl->m_socket == INVALID_SOCKET)
		return false;

	fd_set set;
	FD_ZERO( &set );
	FD_SET( m_impl->m_socket, &set );

	struct timeval timeout = { 0, 0 };
	int selected = select( m_impl->m_socket + 1U, &set, 0, 0, &timeout );
	if (selected < 0)
	{
		Syslogger(m_logContext) << "Disconnect from IsSocketReadReady" ;
		Disconnect();
		return false;
	}
	return (selected != 0 && FD_ISSET( m_impl->m_socket, &set ));
}

bool TcpSocket::SelectRead (const TimePoint &timeout)
{
	if (m_impl->m_socket == INVALID_SOCKET)
		return false;

	bool res= false;
	fd_set selected;
	FD_ZERO( &selected );
	FD_SET( m_impl->m_socket, &selected );
	struct timeval timeoutTV;
	SET_TIMEVAL_US(timeoutTV, timeout);
	res = select( m_impl->m_socket + 1, &selected, 0, 0, &timeoutTV ) > 0 && FD_ISSET( m_impl->m_socket, &selected );
	return res;
}

void TcpSocket::SetBufferSize()
{
	m_recieveBufferSize = m_impl->GetRecieveBuffer();
	if (m_recieveBufferSize < m_params.m_recommendedRecieveBufferSize)
	{
		if (!m_impl->SetRecieveBuffer(m_params.m_recommendedRecieveBufferSize))
		{
			Syslogger(m_logContext, LOG_INFO) << "Failed to set recieve socket buffer size:" << m_params.m_recommendedRecieveBufferSize;
		}
		m_recieveBufferSize = m_impl->GetRecieveBuffer();
	}
	m_sendBufferSize = m_impl->GetSendBuffer();
	if (m_sendBufferSize < m_params.m_recommendedSendBufferSize)
	{
		if (!m_impl->SetSendBuffer(m_params.m_recommendedSendBufferSize))
		{
			Syslogger(m_logContext, LOG_INFO) << "Failed to set send socket buffer size:" << m_params.m_recommendedSendBufferSize;
		}
		m_sendBufferSize = m_impl->GetSendBuffer();
	}
}

bool TcpSocketPrivate::SetBlocking(bool blocking)
{
#if  defined(TCP_SOCKET_WIN )
	u_long flags = blocking ? 0 : 1;
	if(ioctlsocket( m_socket, FIONBIO, &flags ) != NO_ERROR)
	{
		Syslogger(LOG_ERR) << "ioctlsocket(FIONBIO O_NONBLOCK) failed." ;
		return false;
	}
#else
	long flags = fcntl( m_socket, F_GETFL, 0 );
	if (flags < 0)
	{
		Syslogger(LOG_ERR) << "fcntl(F_GETFL) failed." ;
		return false;
	}
	if (!blocking)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if(fcntl( m_socket, F_SETFL, flags ) < 0)
	{
		Syslogger(LOG_ERR) << "fcntl(F_SETFL O_NONBLOCK) failed." ;
		return false;
	}
#endif
	return true;
}

bool TcpSocketPrivate::SetRecieveBuffer(uint32_t size)
{
	int valopt = static_cast<int>(size);
	socklen_t valopt_len = sizeof(valopt);
	return setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, SOCK_OPT_ARG &valopt, valopt_len) == 0;
}

uint32_t TcpSocketPrivate::GetRecieveBuffer()
{
	int valopt = 0;
	socklen_t valopt_len = sizeof(valopt);
	if (getsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, SOCK_OPT_ARG &valopt, &valopt_len))
		return g_defaultBufferSize;

	return static_cast<uint32_t>(valopt);
}
bool TcpSocketPrivate::SetSendBuffer(uint32_t size)
{
	int valopt = static_cast<int>(size);
	socklen_t valopt_len = sizeof(valopt);
	return setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, SOCK_OPT_ARG &valopt, valopt_len) == 0;
}

uint32_t TcpSocketPrivate::GetSendBuffer()
{
	int valopt = 0;
	socklen_t valopt_len = sizeof(valopt);
	if (getsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, SOCK_OPT_ARG &valopt, &valopt_len))
		return g_defaultBufferSize;

	return static_cast<uint32_t>(valopt);
}
}
