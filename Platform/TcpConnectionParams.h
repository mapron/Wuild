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
#include <memory>
#include "TimePoint.h"

namespace Wuild
{
struct TcpConnectionParamsPrivate;
/// Parameters for tcp connection: client or server.
/// Contains information about host, port and timeouts.
struct TcpConnectionParams
{
	TcpConnectionParams();
	~TcpConnectionParams();

	/// Returns true if host resolved correctly.
	bool SetPoint(int port, std::string host = std::string());

	/// Outputs host:port as string.
	std::string GetShortInfo() const;

	int GetPort() const { return m_port; }
	const std::string& GetHost() const { return m_host; }

	TimePoint m_readTimeout = 0.0;                        //!< Scoket read timeout
	TimePoint m_connectTimeout = 1.0;                     //!< Connection timeout
	bool      m_skipFailedConnection = true;              //!< Do not retry recreate listener on fail.
	size_t    m_recommendedRecieveBufferSize = 4 * 1024;  //!< Buffer size is recommended for socket. If socket has lower size, buffer will be optionally increased (bu not ought to)
	size_t    m_recommendedSendBufferSize = 4 * 1024;

	std::shared_ptr<TcpConnectionParamsPrivate> m_impl; //!< making possible to copy params.

private:
	int m_port = 0;
	std::string m_host;
};
}
