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

#include "AppUtils.h"

#include <RemoteToolClient.h>
#include <LocalExecutor.h>
#include <FileUtils.h>
#include <VersionChecker.h>

#include <iostream>

/*
 * Invoke specific tool on remote. No task splitting is performed.
 * WuildToolExecutor clang40_cpp -c test.cpp -o test.o
 */
int main(int argc, char** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "ToolExecutor");

	IInvocationRewriter::Config iconfig;
	if (!app.GetInvocationRewriterConfig( iconfig ))
		return 1;

	auto invocationRewriter = InvocationRewriter::Create(iconfig);

	//Syslogger() << "Configuration: " << app.DumpAllConfigValues();

	app.m_loggerConfig.m_maxLogLevel = Syslogger::Warning;
	app.InitLogging(app.m_loggerConfig);

	StringVector args = app.GetRemainArgs();
	if (args.empty())
	{
		Syslogger(Syslogger::Err) << "usage: ToolExecutor <tool_id> [arguments]";
		return 1;
	}
	const auto toolId = args[0];
	args.erase(args.begin());
	ToolInvocation invocation = invocationRewriter->CompleteInvocation(ToolInvocation(args).SetId(toolId));

	RemoteToolClient::Config config;
	if (!app.GetRemoteToolClientConfig(config))
		return 1;
	
	auto localExecutor = LocalExecutor::Create(invocationRewriter, app.m_tempDir);
	const auto toolsVersions = VersionChecker::Create(localExecutor)->DetermineToolVersions(invocationRewriter);	

	RemoteToolClient rcClient(invocationRewriter, toolsVersions);
	config.m_queueTimeout = TimePoint(5.0);

	if (!rcClient.SetConfig(config))
		return 1;

	rcClient.Start();

	auto callback = []( const Wuild::RemoteToolClient::TaskExecutionInfo& info){
		if (!info.m_stdOutput.empty())
			std::cout << info.m_stdOutput << std::flush;
		Application::Interrupt(1 - info.m_result);
	};
	rcClient.InvokeTool(invocation, callback);

	return ExecAppLoop();
}
