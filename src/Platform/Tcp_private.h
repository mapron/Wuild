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

#ifndef _WIN32

	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <sys/ioctl.h>
	#include <fcntl.h>
	#include <sys/types.h>
	#include <netdb.h>

	#ifndef INVALID_SOCKET
		#define INVALID_SOCKET -1
	#endif
	typedef int SOCKET;
	inline void SocketEngineCheck() {}
	#define SOCK_OPT_ARG (void*)
	#define SocketGetLastError() errno
	#define SocketCheckConnectionPending(err) ((err) == EINPROGRESS)
	#define SocketRWPending(err) ((err) == EAGAIN)
#else // Pure win: ws2_32
	#include <WinSock2.h>
	#include <WS2tcpip.h>
	#include <stdio.h>
	#include <Windows.h>

	#define TCP_SOCKET_WIN
	#define SOCK_OPT_ARG (char*)
	void SocketEngineCheck();
	#define close closesocket
	#define SocketGetLastError() GetLastError()
	#define SocketCheckConnectionPending(err) ((err) == WSAEWOULDBLOCK)
	#define SocketRWPending(err) ((err) == WSAEWOULDBLOCK)
#endif

#include <stdint.h>

#define SET_TIMEVAL_US(timeout, timePoint) \
	timeout.tv_usec =(long)(timePoint.GetUS() % Wuild::TimePoint::ONE_SECOND);\
	timeout.tv_sec = (long)(timePoint.GetUS() / Wuild::TimePoint::ONE_SECOND);

namespace Wuild
{

class TcpSocketPrivate
{
public:
	SOCKET m_socket = INVALID_SOCKET;
	bool SetBlocking(bool blocking);
	bool SetRecieveBuffer(uint32_t size);
	uint32_t GetRecieveBuffer();
	bool SetSendBuffer(uint32_t size);
	uint32_t GetSendBuffer();
	bool SetNoSigPipe();
};

}
