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

#include "Tcp_private.h"

#include <string>

namespace Wuild
{
	class TcpEndPointPrivate
	{
		friend class TcpEndPoint;
	public:
		void FreeAddr() { if (ai) freeaddrinfo(ai); ai = nullptr; }
		~TcpEndPointPrivate() { FreeAddr(); }

		SOCKET MakeSocket() const
		{
			return socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		}
		int Connect(SOCKET sock) const
		{
			return connect(sock, ai->ai_addr, static_cast<int>(ai->ai_addrlen));
		}
		int Bind(SOCKET sock) const
		{
			return ::bind( sock, ai->ai_addr, static_cast<int>(ai->ai_addrlen));
		}
		std::string ToString() const
		{
			sockaddr_in * sockin = (sockaddr_in *)ai->ai_addr;
			return inet_ntoa( sockin->sin_addr ) ;
		}

	private:
		addrinfo * ai = nullptr;
	};
}
