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

#include "TcpConnectionParams.h"
#include "TcpConnectionParams_private.h"
#include "Syslogger.h"

#include <sstream>

namespace Wuild
{

TcpConnectionParams::TcpConnectionParams()
	: m_impl(new TcpConnectionParamsPrivate())
{
	SocketEngineCheck();
}

TcpConnectionParams::~TcpConnectionParams()
{

}

void TcpConnectionParams::SetPoint(int port, std::string host)
{
	m_errorShown = false;
	m_resolved = false;
	m_host = host;
	m_port = port;
}

bool TcpConnectionParams::Resolve()
{
	if (m_resolved)
		return true;

	auto host = m_host;
	bool any = false;
	if (host == "*")
	{
#ifndef __linux__
		host = "";
#endif
		any = true;
	}

	const std::string portStr = std::to_string(m_port);

	struct addrinfo hint = {};
	hint.ai_family = AF_INET;
	hint.ai_protocol = PF_UNSPEC;// PF_INET;
	hint.ai_socktype = SOCK_STREAM;

	m_impl->freeAddr();

	int ret = getaddrinfo(host.c_str(), portStr.c_str(), &hint, &(m_impl->ai));
	if (ret)
	{
		if (!m_errorShown)
		{
			m_errorShown = true;
			Syslogger(LOG_ERR) << "Failed to detect socket information for :" << GetShortInfo() << ", ret=" << ret;
		}
		return false;
	}
	m_impl->ai->ai_protocol = PF_UNSPEC; // workaround for getaddrinfo wrong detection.

	if (any)
	{
		sockaddr_in * sin = (sockaddr_in *)m_impl->ai->ai_addr;
#ifdef _WIN32
		sin->sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#else
		sin->sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	}
	m_resolved = true;
	return true;
}

std::string TcpConnectionParams::GetShortInfo() const
{
	std::ostringstream os;
	os << m_host << ":" << m_port;
	return os.str();
}

}
