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

#include "IDataListener.h"
#include "TcpConnectionParams.h"

namespace Wuild
{

class TcpListenerPrivate;
class TcpSocket;

/// Implementation of TCP socket listener.
class TcpListener : public IDataListener
{
public:
	TcpListener (TcpListenerParams  params);
	~TcpListener ();

	static IDataListener::Ptr Create(const TcpListenerParams & params);

	IDataSocket::Ptr GetPendingConnection() override;
	bool StartListen() override;
	std::string GetLogContext() const override { return m_logContext;}

	/// Accepting TcpSocket, which was created through GetPendingConnection.
	/// TcpSocket calls this, there is no need to call DoAccept manually.
	/// After accept, state of TcpSocket changed from Pending to Connected.
	bool DoAccept(TcpSocket* client);

private:
	bool HasPendingConnections();

	bool IsListenerReadReady ();
	std::unique_ptr<TcpListenerPrivate> m_impl;
	TcpListenerParams m_params;
	std::string m_logContext;

	bool m_listenerFailed = false;
	bool m_waitingAccept = false;
};

}
