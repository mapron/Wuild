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

#include <ConfiguredApplication.h>

#include <RemoteToolClient.h>
#include <CompilerModule.h>
#include <ThreadUtils.h>
#include <Syslogger.h>

#include "remote_executor.h"
#include "debug_flags.h"

#include <iostream>

//#define TEST_CLIENT
#ifdef TEST_CLIENT
#include <RemoteToolServer.h>
#include <LocalExecutor.h>
#endif

class RemoteExecutor: public IRemoteExecutor
{
	Wuild::ConfiguredApplication & m_app;
	Wuild::IInvocationRewriter::Ptr m_compiler;
	Wuild::RemoteToolClient m_remoteService;

#ifdef TEST_CLIENT
	Wuild::ILocalExecutor::Ptr m_localExecutor;
	std::shared_ptr<Wuild::RemoteToolServer> m_toolServer;
#endif

	bool m_remoteEnabled = false;
	bool m_hasStart = false;
	int m_minimalRemoteTasks = 0;

	std::deque<Result> m_results;
	mutable std::mutex m_resultsMutex;

	Wuild::TimePoint m_start;
	Wuild::TimePoint m_totalExecutionTime;
	Wuild::TimePoint m_totalNetworkTime;
	size_t m_remoteTasks = 0;
public:

	RemoteExecutor(Wuild::ConfiguredApplication & app) : m_app(app)
	{
		using namespace Wuild;

		m_start = TimePoint(true);

		bool silent =  !Wuild::Syslogger::IsLogLevelEnabled(LOG_DEBUG);

		IInvocationRewriter::Config compilerConfig;
		if (!m_app.GetInvocationRewriterConfig(compilerConfig, silent))
			return;


		RemoteToolClient::Config remoteToolConfig;
		if (!m_app.GetRemoteToolClientConfig(remoteToolConfig, silent))
			return;

		m_minimalRemoteTasks = remoteToolConfig.m_minimalRemoteTasks;

		m_compiler = InvocationRewriter::Create(compilerConfig);

		if (!m_remoteService.SetConfig(remoteToolConfig))
			return;

#ifdef  TEST_CLIENT
		m_localExecutor = LocalExecutor::Create(m_compiler, m_app.m_tempDir);
		m_app.m_remoteToolServerConfig.m_workersCount = 2;
		m_app.m_remoteToolServerConfig.m_listenHost = "localhost";
		m_app.m_remoteToolServerConfig.m_listenPort = 12345;
		m_app.m_remoteToolServerConfig.m_coordinator.m_enabled = false;

		m_toolServer.reset(new RemoteToolServer(m_localExecutor));
		if (!m_toolServer->SetConfig(m_app.m_remoteToolServerConfig))
			return;

		CoordinatorWorkerInfo workerInfo;
		workerInfo.m_connectionHost = "localhost";
		workerInfo.m_connectionPort = 12345;
		workerInfo.m_toolIds = m_compiler->GetConfig().m_toolIds;
		workerInfo.m_totalThreads = 2;
		m_remoteService.AddClient(workerInfo);

		remoteToolConfig.m_coordinator.m_enabled = false;
		m_remoteService.SetConfig(remoteToolConfig);
#endif

		m_remoteEnabled = true;
	}

	void SetVerbose(bool verbose)
	{
		if (!verbose || Wuild::Syslogger::IsLogLevelEnabled(LOG_INFO))
			return;

		m_app.m_loggerConfig.m_maxLogLevel = LOG_INFO;
		m_app.InitLogging(m_app.m_loggerConfig);
	}


	bool PreprocessCode(const std::vector<std::string> & originalRule,
						const std::vector<std::string> & ignoredArgs,
						std::string & compilerId,
						std::vector<std::string> & preprocessRule,
						std::vector<std::string> & compileRule) const override
	{
		if (!m_remoteEnabled || originalRule.size() < 3)
			return false;

		std::vector<std::string> args = originalRule;

		Wuild::StringVector ppArgs, ccArgs;
		std::string ppFilename, objFilename;
		std::string srcExecutable = Wuild::StringUtils::Trim(args[0]);
		args.erase(args.begin());
		auto space = srcExecutable.find(' ');
		if (space != std::string::npos)
		{
			args.insert(args.begin(), srcExecutable.substr(space + 1));
			srcExecutable = srcExecutable.substr(0, space);
		}
		Wuild::ToolInvocation original, pp, cc;
		original.m_id.m_toolExecutable = srcExecutable;
		original.m_args = args;
		original.m_ignoredArgs = ignoredArgs;
		if (!m_compiler->SplitInvocation(original, pp, cc))
			return false;

		compilerId = pp.m_id.m_toolId;

		preprocessRule.push_back(srcExecutable + "  ");
		preprocessRule.insert(preprocessRule.end(), pp.m_args.begin(), pp.m_args.end());

		compileRule.push_back(srcExecutable + "  ");
		compileRule.insert(compileRule.end(), cc.m_args.begin(), cc.m_args.end());

		return true;
	}
	std::string GetPreprocessedPath(const std::string & sourcePath, const std::string & objectPath) const override
	{
		if (!m_remoteEnabled)
			return "";

		return m_compiler->GetPreprocessedPath(sourcePath, objectPath);
	}
	std::string FilterPreprocessorFlags(const std::string & compilerId, const std::string & flags) const override
	{
		if (!m_remoteEnabled)
			return flags;

		return m_compiler->FilterFlags(Wuild::ToolInvocation(flags, Wuild::ToolInvocation::InvokeType::Preprocess).SetId(compilerId)).GetArgsString(false);
	}

	std::string FilterCompilerFlags(const std::string & compilerId, const std::string & flags) const override
	{
		if (!m_remoteEnabled)
			return flags;

		return m_compiler->FilterFlags(Wuild::ToolInvocation(flags, Wuild::ToolInvocation::InvokeType::Compile).SetId(compilerId)).GetArgsString(false);
	}
	void RunIfNeeded() override
	{
		if (!m_remoteEnabled || m_hasStart)
			return;

		m_hasStart = true;
		m_remoteService.Start();
		#ifdef  TEST_CLIENT
		m_toolServer->Start();
		#endif
	}
	void SleepSome() const  override
	{
		usleep(1000);
	}
	int GetMinimalRemoteTasks() const override
	{
		if (!m_remoteEnabled)
			return -1;

		return m_minimalRemoteTasks;
	}

	bool CanRunMore() override
	{
		if (!m_remoteEnabled)
			return false;

		return m_remoteService.GetFreeRemoteThreads() > 0;
	}

	bool StartCommand(void* userData, const std::string & command)  override
	{
		if (!m_remoteEnabled)
			return false;

		 const auto space = command.find(' ');
		 Wuild::ToolInvocation invocation(command.substr(space + 1));
		 invocation.SetExecutable(command.substr(0, space));

		 invocation = m_compiler->CompleteInvocation(invocation);

		 auto outputFilename = invocation.GetOutput();
		 auto callback = [this, userData, outputFilename]( const Wuild::RemoteToolClient::ExecutionInfo & info)
		 {
			 bool result = info.m_result;
			 // TODO: atomic/mutex!
			 m_totalExecutionTime += info.m_toolExecutionTime;
			 m_totalNetworkTime += info.m_networkRequestTime;

			 Wuild::Syslogger(LOG_INFO) << outputFilename
								 << " -> " << result << ", " <<  info.GetProfilingStr()
									;
			 std::lock_guard<std::mutex> lock(m_resultsMutex);
			 m_results.emplace_back(userData, result, info.m_stdOutput);
		 };
		 m_remoteService.InvokeTool(invocation, callback);
		 m_remoteTasks++;

		 return true;
	}


	/// return true if has finished result.
	bool WaitForCommand(Result* result)  override
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

	void Abort() override
	{
		m_remoteEnabled = false;
	}

	~RemoteExecutor()
	{
		using namespace Wuild;
		if (!m_remoteTasks)
			return;

		auto cus = m_totalExecutionTime.GetUS();
		auto nus = m_totalNetworkTime.GetUS();
		auto overheadPercent = ((nus - cus) * 100) / (cus ? cus : 1);
		Syslogger(LOG_NOTICE) << "Total remote tasks:" << m_remoteTasks << ", done in " << m_start.GetElapsedTime().ToString()
							  <<  " total compilationTime: " << m_totalExecutionTime.ToString(false) << ", "
							  << " total networkTime: "  << m_totalNetworkTime.ToString(false) << ", "
							  << " total overhead: " << overheadPercent << "%"
								  ;

	}

};
