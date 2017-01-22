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

#include "ToolProxyServer.h"

#include <SocketFrameService.h>
#include <FileUtils.h>

namespace Wuild
{

ToolProxyServer::ToolProxyServer(ILocalExecutor::Ptr executor, RemoteToolClient &rcClient)
	: m_executor(executor), m_rcClient(rcClient)
{
	m_executor->SetThreadCount(m_config.m_threadCount);
}

ToolProxyServer::~ToolProxyServer()
{
	m_server.reset();
}

bool ToolProxyServer::SetConfig(const ToolProxyServer::Config &config)
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


void ToolProxyServer::Start()
{
	m_server.reset(new SocketFrameService( m_config.m_listenPort ));
	m_server->RegisterFrameReader(SocketFrameReaderTemplate<ToolProxyRequest>::Create([this](const ToolProxyRequest& inputMessage, SocketFrameHandler::OutputCallback outputCallback){

		// TODO: we assume that proxy server is used to build only one working directory at once.
		if (m_cwd != inputMessage.m_cwd)
		{
			m_cwd = inputMessage.m_cwd;
			SetCWD(m_cwd);
		}

		LocalExecutorTask::Ptr original(new LocalExecutorTask());
		original->m_invocation = inputMessage.m_invocation;
		original->m_readOutput = original->m_writeInput = false;
		std::string err;
		ILocalExecutor::TaskPair tasks = m_executor->SplitTask(original, err);
		if (tasks.first)
		{
			LocalExecutorTask::Ptr taskPP = tasks.first;

			taskPP->m_callback = [&rcClient=m_rcClient, &executor=m_executor, taskCC=tasks.second, outputCallback] ( LocalExecutorResult::Ptr localResult ) {

				if (!localResult->m_result)
				{
					outputCallback(ToolProxyResponse::Ptr(new ToolProxyResponse(localResult->m_stdOut)));
					return;
				}

				if (rcClient.GetFreeRemoteThreads() > 0)
				{
					auto remoteCallback = [outputCallback]( const Wuild::RemoteToolClient::TaskExecutionInfo& info) {
						outputCallback(ToolProxyResponse::Ptr(new ToolProxyResponse(info.m_stdOutput, info.m_result)));
					};
					rcClient.InvokeTool(taskCC->m_invocation, remoteCallback);
				}
				else
				{
					taskCC->m_callback = [outputCallback]( LocalExecutorResult::Ptr localResult ) {
						outputCallback(ToolProxyResponse::Ptr(new ToolProxyResponse(localResult->m_stdOut, localResult->m_result)));
					};
					executor->AddTask(taskCC);
				}
			};
			m_executor->AddTask(taskPP);
		}
		else
		{
			original->m_callback = [outputCallback] ( LocalExecutorResult::Ptr localResult ) {
				outputCallback(ToolProxyResponse::Ptr(new ToolProxyResponse(localResult->m_stdOut, localResult->m_result)));
			};
			m_executor->AddTask(original);
		}
	}));

	m_server->Start();
}
}
