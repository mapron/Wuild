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

#include "remote_executor_impl.h"

using namespace Wuild;

RemoteExecutor::RemoteExecutor(ConfiguredApplication &app) : m_app(app)
{
	bool silent = !Syslogger::IsLogLevelEnabled(LOG_DEBUG);

	IInvocationRewriter::Config compilerConfig;
	if (!m_app.GetInvocationRewriterConfig(compilerConfig, silent))
		return;

	RemoteToolClient::Config remoteToolConfig;
	if (!m_app.GetRemoteToolClientConfig(remoteToolConfig, silent))
		return;

	m_minimalRemoteTasks = remoteToolConfig.m_minimalRemoteTasks;

	m_invocationRewriter = InvocationRewriter::Create(compilerConfig);
	m_remoteService.reset(new RemoteToolClient(m_invocationRewriter));
	if (!m_remoteService->SetConfig(remoteToolConfig))
		return;

#ifdef  TEST_CLIENT
	m_localExecutor = LocalExecutor::Create(m_invocationRewriter, m_app.m_tempDir);
	m_app.m_remoteToolServerConfig.m_threadCount = 2;
	m_app.m_remoteToolServerConfig.m_listenHost = "localhost";
	m_app.m_remoteToolServerConfig.m_listenPort = 12345;
	m_app.m_remoteToolServerConfig.m_coordinator.m_enabled = false;

	m_toolServer.reset(new RemoteToolServer(m_localExecutor));
	if (!m_toolServer->SetConfig(m_app.m_remoteToolServerConfig))
		return;

	ToolServerInfo toolServerInfo;
	toolServerInfo.m_connectionHost = "localhost";
	toolServerInfo.m_connectionPort = 12345;
	toolServerInfo.m_toolIds = m_compiler->GetConfig().m_toolIds;
	toolServerInfo.m_totalThreads = 2;
	m_remoteService->AddClient(toolServerInfo);

	remoteToolConfig.m_coordinator.m_enabled = false;
	m_remoteService->SetConfig(remoteToolConfig);
#endif

	m_remoteEnabled = true;
}

void RemoteExecutor::SetVerbose(bool verbose)
{
	if (!verbose || Syslogger::IsLogLevelEnabled(LOG_INFO))
		return;

	m_app.m_loggerConfig.m_maxLogLevel = LOG_INFO;
	m_app.InitLogging(m_app.m_loggerConfig);
}

bool RemoteExecutor::PreprocessCode(const std::vector<std::string> &originalRule, const std::vector<std::string> &ignoredArgs, std::string &toolId, std::vector<std::string> &preprocessRule, std::vector<std::string> &compileRule) const
{
	if (!m_remoteEnabled || originalRule.size() < 3)
		return false;

	std::vector<std::string> args = originalRule;

	std::string srcExecutable = StringUtils::Trim(args[0]);
	args.erase(args.begin());
	auto space = srcExecutable.find(' ');
	if (space != std::string::npos)
	{
		args.insert(args.begin(), srcExecutable.substr(space + 1));
		srcExecutable = srcExecutable.substr(0, space);
	}
	ToolInvocation original, pp, cc;
	original.m_id.m_toolExecutable = srcExecutable;
	original.m_args = args;
	original.m_ignoredArgs = ignoredArgs;
	if (!m_invocationRewriter->SplitInvocation(original, pp, cc))
		return false;

	toolId = pp.m_id.m_toolId;

	preprocessRule.push_back(srcExecutable + "  ");
	preprocessRule.insert(preprocessRule.end(), pp.m_args.begin(), pp.m_args.end());

	compileRule.push_back(srcExecutable + "  ");
	compileRule.insert(compileRule.end(), cc.m_args.begin(), cc.m_args.end());

	return true;
}

std::string RemoteExecutor::GetPreprocessedPath(const std::string &sourcePath, const std::string &objectPath) const
{
	if (!m_remoteEnabled)
		return "";

	return m_invocationRewriter->GetPreprocessedPath(sourcePath, objectPath);
}

std::string RemoteExecutor::FilterPreprocessorFlags(const std::string &toolId, const std::string &flags) const
{
	if (!m_remoteEnabled)
		return flags;

	return m_invocationRewriter->FilterFlags(ToolInvocation(flags, ToolInvocation::InvokeType::Preprocess).SetId(toolId)).GetArgsString(false);
}

std::string RemoteExecutor::FilterCompilerFlags(const std::string &toolId, const std::string &flags) const
{
	if (!m_remoteEnabled)
		return flags;

	return m_invocationRewriter->FilterFlags(ToolInvocation(flags, ToolInvocation::InvokeType::Compile).SetId(toolId)).GetArgsString(false);
}

void RemoteExecutor::RunIfNeeded(const std::vector<std::string> &toolIds)
{
	if (!m_remoteEnabled || m_hasStart)
		return;

	m_hasStart = true;
	m_remoteService->Start(toolIds);
#ifdef  TEST_CLIENT
	m_toolServer->Start();
#endif
}

void RemoteExecutor::SleepSome() const
{
	usleep(1000);
}

int RemoteExecutor::GetMinimalRemoteTasks() const
{
	if (!m_remoteEnabled)
		return -1;

	return m_minimalRemoteTasks;
}

bool RemoteExecutor::CanRunMore()
{
	if (!m_remoteEnabled)
		return false;

	return m_remoteService->GetFreeRemoteThreads() > 0;
}

bool RemoteExecutor::StartCommand(void *userData, const std::string &command)
{
	if (!m_remoteEnabled)
		return false;

	const auto space = command.find(' ');
	ToolInvocation invocation(command.substr(space + 1));
	invocation.SetExecutable(command.substr(0, space));

	invocation = m_invocationRewriter->CompleteInvocation(invocation);

	auto outputFilename = invocation.GetOutput();
	auto callback = [this, userData, outputFilename]( const RemoteToolClient::TaskExecutionInfo & info)
	{
		bool result = info.m_result;
		Syslogger() << outputFilename<< " -> " << result << ", " <<  info.GetProfilingStr() ;
		std::lock_guard<std::mutex> lock(m_resultsMutex);
		m_results.emplace_back(userData, result, info.m_stdOutput);
	};
	m_remoteService->InvokeTool(invocation, callback);

	return true;
}

bool RemoteExecutor::WaitForCommand(IRemoteExecutor::Result *result)
{
	if (!m_remoteEnabled)
		return false;

	std::lock_guard<std::mutex> lock(m_resultsMutex);
	if (!m_results.empty())
	{
		*result = std::move(m_results[0]);
		m_results.pop_front();
		return true;
	}
	return false;
}

RemoteExecutor::~RemoteExecutor()
{
	if (!m_remoteEnabled)
		return;

	m_remoteService->FinishSession();
	Syslogger(LOG_NOTICE) <<  m_remoteService->GetSessionInformation();

}
