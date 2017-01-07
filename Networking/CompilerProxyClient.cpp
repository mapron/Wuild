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

#include "CompilerProxyClient.h"

#include "CompilerProxyFrames.h"

#include <Syslogger.h>
#include <SocketFrameHandler.h>
#include <Application.h>

#include <iostream>

namespace Wuild
{

CompilerProxyClient::CompilerProxyClient()
{
}

CompilerProxyClient::~CompilerProxyClient()
{
	if (m_client)
		m_client->Stop();
}

bool CompilerProxyClient::SetConfig(const CompilerProxyClient::Config &config)
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

void CompilerProxyClient::Start()
{
	m_client.reset(new SocketFrameHandler( ));
	m_client->RegisterFrameReader(SocketFrameReaderTemplate<ProxyResponse>::Create());

	m_client->SetTcpChannel("localhost", m_config.m_listenPort);

	m_client->Start();

}

void CompilerProxyClient::RunTask(const StringVector &args)
{
	auto frameCallback = [](SocketFrame::Ptr responseFrame, SocketFrameHandler::TReplyState state)
	{
		std::string stdOut;
		bool result = false;
		if (state == SocketFrameHandler::rsTimeout)
		{
			stdOut = "Timeout expired.";
		}
		else if (state == SocketFrameHandler::rsError)
		{
			stdOut ="Internal error.";
		}
		else
		{
			ProxyResponse::Ptr responseFrameProxy = std::dynamic_pointer_cast<ProxyResponse>(responseFrame);
			result = responseFrameProxy->m_result;
			stdOut = responseFrameProxy->m_stdOut;
		}
		if (!stdOut.empty())
			std::cerr << stdOut << std::endl;
		Application::Interrupt(1 - result);
	};
	ProxyRequest::Ptr req(new ProxyRequest());
	req->m_invocation.m_id.m_compilerId = m_config.m_toolId;
	req->m_invocation.m_args = args;
	m_client->QueueFrame(req, frameCallback);
}

}
