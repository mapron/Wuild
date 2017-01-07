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

#include "CompilerProxyServer.h"

#include "SocketFrameService.h"

namespace Wuild
{

CompilerProxyServer::CompilerProxyServer(ILocalExecutor::Ptr executor, RemoteToolClient &rcClient)
	: m_executor(executor), m_rcClient(rcClient)
{

}

CompilerProxyServer::~CompilerProxyServer()
{
	m_server.reset();
}

bool CompilerProxyServer::SetConfig(const CompilerProxyServer::Config &config)
{
	std::ostringstream os;
	if (!config.Validate(&os))
	{
		Syslogger(LOG_ERR) << os.str();
		return false;
	}
	m_config = config;
	return true;
}


void CompilerProxyServer::Start()
{
	m_server.reset(new SocketFrameService( m_config.m_listenPort ));
	m_server->RegisterFrameReader(SocketFrameReaderTemplate<ProxyRequest>::Create([this](const ProxyRequest& inputMessage, SocketFrameHandler::OutputCallback outputCallback){

		ProxyResponse::Ptr response(new ProxyResponse());
		response->m_result = false;
		response->m_stdOut = "FFFOOOO!";
		outputCallback(response);
	}));


	m_server->SetHandlerDestroyCallback([this](SocketFrameHandler * handler){

	});
	m_server->Start();
}
}
