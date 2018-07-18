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

#include <CommonTypes.h>
#include <Syslogger.h>
#include <InvocationRewriterConfig.h>
#include <LoggerConfig.h>
#include <RemoteToolClientConfig.h>
#include <RemoteToolServerConfig.h>
#include <CoordinatorServerConfig.h>
#include <CoordinatorClientConfig.h>
#include <ToolProxyServerConfig.h>

#include <algorithm>

namespace Wuild
{
class AbstractConfig;
/// Contains prepared configs for different services
class ConfiguredApplication
{
public:
	LoggerConfig m_loggerConfig;
	InvocationRewriterConfig m_invocationRewriterConfig;
	RemoteToolClientConfig m_remoteToolClientConfig;
	RemoteToolServerConfig m_remoteToolServerConfig;
	CoordinatorServerConfig m_coordinatorServerConfig;
	ToolProxyServerConfig m_toolProxyServerConfig;

	std::string m_tempDir;

	/// parses paramenter from commandline, reading wuild configuration, filling Config structures.
	ConfiguredApplication(int argc, char** argv, const std::string& appName = std::string(), std::string  defaultGroupName = std::string());
	~ConfiguredApplication();

	const StringVector & GetRemainArgs() const { return m_remainArgs;}
	int GetRemainArgc() const;
	char** GetRemainArgv() const;

	bool InitLogging(const LoggerConfig & loggerConfig);

	bool GetInvocationRewriterConfig(InvocationRewriterConfig & config, bool silent = false) const;
	bool GetCoordinatorServerConfig(CoordinatorServerConfig & config) const;
	bool GetToolProxyServerConfig(ToolProxyServerConfig & config) const;

	bool GetRemoteToolClientConfig(RemoteToolClientConfig & config, bool silent = false) const;
	bool GetRemoteToolServerConfig(RemoteToolServerConfig & config, bool silent = false) const;

	std::string DumpAllConfigValues() const;

private:
	StringVector m_remainArgs;
	std::vector<char*> m_remainArgv;
	std::unique_ptr<AbstractConfig> m_config;
	std::string m_defaultGroupName;
	ConfiguredApplication(const ConfiguredApplication& ) = delete;

	void ReadLoggingConfig();
	void ReadInvocationRewriterConfig();
	void ReadRemoteToolClientConfig();
	void ReadRemoteToolServerConfig();
	void ReadCoordinatorClientConfig(CoordinatorClientConfig & config, const std::string & groupName);
	void ReadCoordinatorServerConfig();
	void ReadCompressionConfig(CompressionInfo & compressionInfo, const std::string & groupName);
	void ReadToolProxyServerConfig();
};
}
