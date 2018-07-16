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

#include <Syslogger_private.h>
#include "ConfiguredApplication.h"

#include "AbstractConfig.h"

#include <Application.h>
#include <StringUtils.h>
#include <Syslogger.h>
#include <FileUtils.h>

namespace
{
	const std::string g_commandLinePrefix = "--wuild-";
	const std::string g_defaultConfigSubfolder =
		#ifndef _WIN32
			".Wuild/"
		#else
			""
		#endif
			;
	const std::string g_defaultConfig = "Wuild.ini";
	const std::string g_envConfig = "WUILD_CONFIG";
}

namespace Wuild
{

ConfiguredApplication::ConfiguredApplication(int argc, char **argv, const std::string &appName, const std::string &defaultGroupName)
	: m_defaultGroupName(defaultGroupName)
{
	if (!appName.empty())
		Application::Instance().SetAppName(appName);

	m_config.reset(new AbstractConfig());
	AbstractConfig & config = *m_config;
	const auto inputArgs = StringUtils::StringVectorFromArgv(argc, argv);
	m_remainArgs = config.ReadCommandLine(inputArgs, g_commandLinePrefix);
	m_remainArgv.resize(m_remainArgs.size() + 2);
	m_remainArgv[0] = argv[0];
	for (size_t i = 0; i < m_remainArgs.size(); ++i)
		m_remainArgv[i+1] = const_cast<char*>(m_remainArgs[i].data()); // waiting for non-const data().

	m_remainArgv[m_remainArgv.size() - 1] = nullptr;

	std::string configPath = config.GetString("", "config");
	if (configPath.empty())
	{
		char *envPath = getenv(g_envConfig.c_str());
		if (envPath)
			configPath = envPath;
	}
	bool unexistendConfigIsError = true;
	if (configPath.empty())
	{
		unexistendConfigIsError = false;
		const std::vector<std::string> configPaths {
			Application::Instance().GetHomeDir() + "/" + g_defaultConfigSubfolder + g_defaultConfig,
			Application::Instance().GetExecutablePath()	+ "/" + g_defaultConfig,
		};
		for (const auto & path : configPaths)
			if (FileInfo(path).Exists())
			{
				configPath = path;
				break;
			}
	}
	if (!config.ReadIniFile(configPath) && unexistendConfigIsError)
	{
		Syslogger(Syslogger::Err) << "Failed to load:" << configPath;
	}
	// if global variable is set in inifile, override in again with commandline
	config.ReadCommandLine(inputArgs, g_commandLinePrefix);

	ReadLoggingConfig();
	ReadInvocationRewriterConfig();
	ReadRemoteToolClientConfig();
	ReadRemoteToolServerConfig();
	ReadCoordinatorServerConfig();
	ReadToolProxyServerConfig();

	InitLogging(m_loggerConfig);

	m_tempDir = m_config->GetString(m_defaultGroupName, "tempDir", Application::Instance().GetTempDir());
}

ConfiguredApplication::~ConfiguredApplication()
{
}

int ConfiguredApplication::GetRemainArgc() const
{
	return m_remainArgv.size() - 1;
}

char **ConfiguredApplication::GetRemainArgv() const
{
	return const_cast<char **>(m_remainArgv.data());
}

bool ConfiguredApplication::InitLogging(const LoggerConfig &loggerConfig)
{
	std::ostringstream os;
	if (loggerConfig.Validate(&os))
	{
		std::unique_ptr<ILoggerBackend> backend;
		switch (loggerConfig.m_logType)
		{
		case LoggerConfig::LogType::Syslog:
	#ifdef __linux__
			backend.reset(new LoggerBackendSyslog(loggerConfig.m_maxLogLevel,
												  loggerConfig.m_outputLoglevel,
												  loggerConfig.m_outputTimestamp,
												  loggerConfig.m_outputTimeoffsets));
			break;
	#endif
			//fallthrough
		case LoggerConfig::LogType::Cout:
		case LoggerConfig::LogType::Cerr:
		case LoggerConfig::LogType::Printf:
			backend.reset(new LoggerBackendConsole(loggerConfig.m_maxLogLevel,
												loggerConfig.m_outputLoglevel,
												loggerConfig.m_outputTimestamp,
												loggerConfig.m_outputTimeoffsets,
												loggerConfig.m_logType == LoggerConfig::LogType::Printf ? LoggerBackendConsole::Type::Printf :
													(loggerConfig.m_logType == LoggerConfig::LogType::Cerr ?  LoggerBackendConsole::Type::Cerr :  LoggerBackendConsole::Type::Cout)
												   ));
		break;
		case LoggerConfig::LogType::Files:
			backend.reset(new LoggerBackendFiles(loggerConfig.m_maxLogLevel,
												 loggerConfig.m_outputLoglevel,
												 loggerConfig.m_outputTimestamp,
												 loggerConfig.m_outputTimeoffsets,
												 loggerConfig.m_maxFilesInDir,
												 loggerConfig.m_maxMessagesInFile,
												 loggerConfig.m_dir));
			break;
		default:
			break;
		}
		if (backend)
			Syslogger::SetLoggerBackend(std::move(backend));
	}
	else
	{
		Syslogger(Syslogger::Err) << os.str();
	}
	return true;
}

bool ConfiguredApplication::GetInvocationRewriterConfig(InvocationRewriterConfig & config, bool silent) const
{
	// TODO: make template, remove copy-paste.
	std::ostringstream os;
	if (!m_invocationRewriterConfig.Validate(&os))
	{
		if (!silent)
			Syslogger(Syslogger::Err) << os.str();
		return false;
	}
	config = m_invocationRewriterConfig;
	return true;
}

bool ConfiguredApplication::GetCoordinatorServerConfig(CoordinatorServerConfig &config) const
{
	// TODO: make template, remove copy-paste.
	std::ostringstream os;
	if (!m_coordinatorServerConfig.Validate(&os))
	{
		Syslogger(Syslogger::Err) << os.str();
		return false;
	}
	config = m_coordinatorServerConfig;
	return true;
}

bool ConfiguredApplication::GetToolProxyServerConfig(ToolProxyServerConfig &config) const
{
	std::ostringstream os;
	if (!m_toolProxyServerConfig.Validate(&os))
	{
		Syslogger(Syslogger::Err) << os.str();
		return false;
	}
	config = m_toolProxyServerConfig;
	return true;
}

bool ConfiguredApplication::GetRemoteToolClientConfig(RemoteToolClientConfig &config, bool silent) const
{
	std::ostringstream os;
	if (!m_remoteToolClientConfig.Validate(&os))
	{
		if (!silent)
			Syslogger(Syslogger::Err) << os.str();
		return false;
	}
	config = m_remoteToolClientConfig;
	return true;
}

bool ConfiguredApplication::GetRemoteToolServerConfig(RemoteToolServerConfig &config, bool silent) const
{
	std::ostringstream os;
	if (!m_remoteToolServerConfig.Validate(&os))
	{
		if (!silent)
			Syslogger(Syslogger::Err) << os.str();
		return false;
	}
	config = m_remoteToolServerConfig;
	return true;
}

std::string ConfiguredApplication::DumpAllConfigValues() const
{
	return m_config->DumpAllValues();
}

void ConfiguredApplication::ReadCoordinatorClientConfig(CoordinatorClientConfig &config, const std::string &groupName)
{
	config.m_coordinatorPort = m_config->GetInt(groupName, "coordinatorPort");
	config.m_coordinatorHost = m_config->GetStringList(groupName, "coordinatorHost");
	config.m_enabled = m_config->GetBool(groupName, "coordinatorEnabled", true);
	int sendInfoIntervalMS = m_config->GetInt(groupName, "sendInfoIntervalMS", 15000);
	config.m_sendInfoInterval = TimePoint(sendInfoIntervalMS / 1000.);
}

void ConfiguredApplication::ReadLoggingConfig()
{
	m_loggerConfig.m_maxLogLevel		= m_config->GetInt (m_defaultGroupName, "logLevel"			, m_loggerConfig.m_maxLogLevel);
	m_loggerConfig.m_maxMessagesInFile	= m_config->GetInt (m_defaultGroupName, "maxMessagesInFile"	, m_loggerConfig.m_maxMessagesInFile);
	m_loggerConfig.m_maxFilesInDir		= m_config->GetInt (m_defaultGroupName, "maxFilesInDir"		, m_loggerConfig.m_maxFilesInDir);
	m_loggerConfig.m_outputTimestamp	= m_config->GetBool(m_defaultGroupName, "outputTimestamp"	, m_loggerConfig.m_outputTimestamp);
	m_loggerConfig.m_outputTimeoffsets	= m_config->GetBool(m_defaultGroupName, "outputTimeoffsets"	, m_loggerConfig.m_outputTimeoffsets);
	m_loggerConfig.m_outputLoglevel		= m_config->GetBool(m_defaultGroupName, "outputLoglevel"	, m_loggerConfig.m_outputLoglevel);

	if (m_config->GetBool(m_defaultGroupName, "logToFile"))
	{
		m_loggerConfig.m_logType = LoggerConfig::LogType::Files;
		m_loggerConfig.m_dir = m_config->GetString(m_defaultGroupName, "logDir");
		if (m_loggerConfig.m_dir.empty())
			m_loggerConfig.m_dir = Application::Instance().GetAppDataDir() + "/Logs";
	}

	if (m_config->GetBool(m_defaultGroupName, "logToSyslog"))
		m_loggerConfig.m_logType = LoggerConfig::LogType::Syslog;
	else if (m_config->GetBool(m_defaultGroupName, "logToCerr"))
		m_loggerConfig.m_logType = LoggerConfig::LogType::Cerr;
}

void ConfiguredApplication::ReadInvocationRewriterConfig()
{
	const std::string defaultGroup("tools");

	m_invocationRewriterConfig.m_toolIds = m_config->GetStringList(defaultGroup, "toolIds");
	for (const auto & id : m_invocationRewriterConfig.m_toolIds)
	{
		InvocationRewriterConfig::Tool unit;
		unit.m_id = id;
		std::string append = m_config->GetString(defaultGroup, id + "_appendRemote");
		if (!append.empty())
		{
			unit.m_appendRemote = " " + append + " ";
		}
		unit.m_removeRemote= m_config->GetString(defaultGroup, id + "_removeRemote");
		unit.m_remoteAlias = m_config->GetString(defaultGroup, id + "_remoteAlias");

		std::string type = m_config->GetString(defaultGroup, id + "_type", "gcc"); // "gcc" or "msvc"
		if (type == "msvc" || unit.m_id.find("ms") == 0)
			unit.m_type = InvocationRewriterConfig::ToolchainType::MSVC;
		else if (type == "gcc")
			unit.m_type = InvocationRewriterConfig::ToolchainType::GCC;
		else if (type == "update_file")
			unit.m_type = InvocationRewriterConfig::ToolchainType::UpdateFile;

		for (auto executable: m_config->GetStringList(defaultGroup, id))
		{
			executable = FileInfo::ToPlatformPath(executable);
			const std::string shortName = FileInfo(executable).GetPlatformShortName();
			if (shortName != executable)
				unit.m_names.push_back(shortName);
			unit.m_names.push_back(executable);
		}

		m_invocationRewriterConfig.m_tools.push_back(unit);
	}
}

void ConfiguredApplication::ReadRemoteToolClientConfig()
{
	const std::string defaultGroup("toolClient");
	m_remoteToolClientConfig.m_invocationAttempts = m_config->GetInt(defaultGroup, "invocationAttempts", m_remoteToolClientConfig.m_invocationAttempts);
	m_remoteToolClientConfig.m_minimalRemoteTasks = m_config->GetInt(defaultGroup, "minimalRemoteTasks", m_remoteToolClientConfig.m_minimalRemoteTasks);
	m_remoteToolClientConfig.m_maxLoadAverage     = m_config->GetDouble(defaultGroup, "maxLoadAverage" , m_remoteToolClientConfig.m_maxLoadAverage);

	int queueTimeoutMS = m_config->GetInt(defaultGroup, "queueTimeoutMS");
	if (queueTimeoutMS)
		m_remoteToolClientConfig.m_queueTimeout = TimePoint(queueTimeoutMS / 1000.);

	int requestTimeoutMS = m_config->GetInt(defaultGroup, "requestTimeoutMS");
	if (requestTimeoutMS)
		m_remoteToolClientConfig.m_requestTimeout = TimePoint(requestTimeoutMS / 1000.);

	ReadCoordinatorClientConfig(m_remoteToolClientConfig.m_coordinator, defaultGroup);
	m_remoteToolClientConfig.m_coordinator.m_redundance = CoordinatorClientConfig::Redundance::Any;
	ReadCompressionConfig(m_remoteToolClientConfig.m_compression, defaultGroup);

	auto & toolServers    = m_remoteToolClientConfig.m_initialToolServers;
	toolServers.m_hosts   = m_config->GetStringList(defaultGroup, "toolserverHosts");
	toolServers.m_port    = m_config->GetInt       (defaultGroup, "toolserverPort");
	toolServers.m_toolIds = m_config->GetStringList(defaultGroup, "toolserverIds");
}

void ConfiguredApplication::ReadRemoteToolServerConfig()
{
	const std::string defaultGroup("toolServer");
	m_remoteToolServerConfig.m_listenPort   = m_config->GetInt(defaultGroup,    "listenPort");
	m_remoteToolServerConfig.m_listenHost   = m_config->GetString(defaultGroup, "listenHost");
	m_remoteToolServerConfig.m_threadCount  = m_config->GetInt(defaultGroup,    "threadCount");
	m_remoteToolServerConfig.m_serverName   = m_config->GetString(defaultGroup, "serverName");
	ReadCoordinatorClientConfig(m_remoteToolServerConfig.m_coordinator, defaultGroup);
	ReadCompressionConfig(m_remoteToolServerConfig.m_compression, defaultGroup);
}

void ConfiguredApplication::ReadCoordinatorServerConfig()
{
	const std::string defaultGroup("coordinator");
	m_coordinatorServerConfig.m_listenPort = m_config->GetInt(defaultGroup, "listenPort");
}

void ConfiguredApplication::ReadCompressionConfig(CompressionInfo &compressionInfo, const std::string &groupName)
{
	auto type = m_config->GetString(groupName,    "compressionType", "Gzip");
	if (type == "None")
		compressionInfo.m_type = CompressionType::None;
	else if (type == "LZ4")
		compressionInfo.m_type = CompressionType::LZ4;
	else if (type == "Gzip")
		compressionInfo.m_type = CompressionType::Gzip;
	else
		Syslogger(Syslogger::Err) << "Invalid compression type:" << type;
	compressionInfo.m_level =  m_config->GetInt(groupName,    "compressionLevel", 5);
}

void ConfiguredApplication::ReadToolProxyServerConfig()
{
	const std::string defaultGroup("proxy");
	m_toolProxyServerConfig.m_listenPort   = m_config->GetInt   (defaultGroup, "listenPort");
	m_toolProxyServerConfig.m_toolId       = m_config->GetString(defaultGroup, "toolId");
	m_toolProxyServerConfig.m_threadCount  = m_config->GetInt   (defaultGroup, "threadCount", m_toolProxyServerConfig.m_threadCount);

	int proxyClientTimeoutMS = m_config->GetInt(defaultGroup, "proxyClientTimeoutMS");
	if (proxyClientTimeoutMS)
		m_toolProxyServerConfig.m_proxyClientTimeout = TimePoint(proxyClientTimeoutMS / 1000.);
}

}
