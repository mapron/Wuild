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

#include "ToolProxyClient.h"

#include "ToolProxyFrames.h"

#include <Syslogger.h>
#include <SocketFrameHandler.h>
#include <Application.h>
#include <FileUtils.h>

#include <algorithm>
#include <iostream>

namespace Wuild
{

ToolProxyClient::ToolProxyClient()
{
}

ToolProxyClient::~ToolProxyClient()
{
	if (m_client)
		m_client->Stop();
}

bool ToolProxyClient::SetConfig(const ToolProxyClient::Config &config)
{
	std::ostringstream os;
	if (!config.Validate(&os))
	{
		Syslogger(Syslogger::Err) << os.str();
		return false;
	}
	m_config = config;
	return true;
}

void ToolProxyClient::Start()
{
	SocketFrameHandlerSettings settings;
	settings.m_writeFailureLogLevel = Syslogger::Info;
	m_client.reset(new SocketFrameHandler( settings ));
	m_client->RegisterFrameReader(SocketFrameReaderTemplate<ToolProxyResponse>::Create());

	m_client->SetTcpChannel("localhost", m_config.m_listenPort);

	m_client->Start();

}

void ToolProxyClient::RunTask(const StringVector &args)
{
	auto frameCallback = [](SocketFrame::Ptr responseFrame, SocketFrameHandler::ReplyState state)
	{
		std::string stdOut;
		bool result = false;
		if (state == SocketFrameHandler::ReplyState::Timeout)
		{
			stdOut = "Timeout expired.";
		}
		else if (state == SocketFrameHandler::ReplyState::Error)
		{
			stdOut = "Internal error.";
		}
		else
		{
			ToolProxyResponse::Ptr responseFrameProxy = std::dynamic_pointer_cast<ToolProxyResponse>(responseFrame);
			result = responseFrameProxy->m_result;
			stdOut = responseFrameProxy->m_stdOut;
		}
		std::replace(stdOut.begin(), stdOut.end(), '\r', ' ');
		if (!stdOut.empty())
			std::cerr << stdOut << std::endl << std::flush;
		Application::Interrupt(1 - result);
	};
	ToolProxyRequest::Ptr req(new ToolProxyRequest());
	req->m_invocation.m_id.m_toolId = m_config.m_toolId;
	req->m_invocation.m_args = args;
	req->m_cwd = GetCWD();
	m_client->QueueFrame(req, frameCallback, m_config.m_proxyClientTimeout);
}

}
