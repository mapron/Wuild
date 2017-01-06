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

namespace
{
    const std::string g_commandLinePrefix = "--wuild-";
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
    m_remainArgv.resize(m_remainArgs.size() + 1);
    m_remainArgv[0] = argv[0];
    for (size_t i = 0; i < m_remainArgs.size(); ++i)
        m_remainArgv[i+1] = const_cast<char*>(m_remainArgs[i].data()); // waiting for non-const data().

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
        configPath = Application::Instance().GetHomeDir() + "/" + g_defaultConfig;
    }
    if (!config.ReadIniFile(configPath) && unexistendConfigIsError)
    {
        Syslogger(LOG_ERR) << "Failed to load:" << configPath;
    }
    // if global variable is set in inifile, override in again with commandline
    config.ReadCommandLine(inputArgs, g_commandLinePrefix);

    ReadLoggingConfig();
    ReadCompilerConfig();
    ReadRemoteToolClientConfig();
    ReadRemoteToolServerConfig();
    ReadCoordinatorServerConfig();
    ReadCompilerProxyServerConfig();

    InitLogging(m_loggerConfig);

    m_tempDir = m_config->GetString(m_defaultGroupName, "tempDir", Application::Instance().GetTempDir());
}

ConfiguredApplication::~ConfiguredApplication()
{
}

int ConfiguredApplication::GetRemainArgc() const
{
    return m_remainArgv.size();
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
        case LoggerConfig::LogType::Cerr:
            backend.reset(new LoggerBackendCerr(loggerConfig.m_maxLogLevel,
                                                loggerConfig.m_outputLoglevel,
                                                loggerConfig.m_outputTimestamp,
                                                loggerConfig.m_outputTimeoffsets));
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
        Syslogger(LOG_ERR) << os.str();
    }
    return true;
}

bool ConfiguredApplication::GetCompilerConfig(CompilerConfig & config, bool silent) const
{
    // TODO: make template, remove copy-paste.
    std::ostringstream os;
    if (!m_compilerConfig.Validate(&os))
    {
        if (!silent)
            Syslogger(LOG_ERR) << os.str();
        return false;
    }
    config = m_compilerConfig;
    return true;
}

bool ConfiguredApplication::GetCoordinatorServerConfig(CoordinatorServerConfig &config) const
{
    // TODO: make template, remove copy-paste.
    std::ostringstream os;
    if (!m_coordinatorServerConfig.Validate(&os))
    {
        Syslogger(LOG_ERR) << os.str();
        return false;
    }
    config = m_coordinatorServerConfig;
    return true;
}

bool ConfiguredApplication::GetCompilerProxyServerConfig(CompilerProxyServerConfig &config) const
{
    std::ostringstream os;
    if (!m_compilerProxyServerConfig.Validate(&os))
    {
        Syslogger(LOG_ERR) << os.str();
        return false;
    }
    config = m_compilerProxyServerConfig;
    return true;
}

bool ConfiguredApplication::GetRemoteToolClientConfig(RemoteToolClientConfig &config, bool silent) const
{
    std::ostringstream os;
    if (!m_remoteToolClientConfig.Validate(&os))
    {
        if (!silent)
            Syslogger(LOG_ERR) << os.str();
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
            Syslogger(LOG_ERR) << os.str();
        return false;
    }
    config = m_remoteToolServerConfig;
    return true;
}

void ConfiguredApplication::ReadCoordinatorClientConfig(CoordinatorClientConfig &config, const std::string &groupName)
{
    config.m_coordinatorPort = m_config->GetInt(groupName, "coordinatorPort");
    config.m_coordinatorHost = m_config->GetString(groupName, "coordinatorHost");
    config.m_enabled = m_config->GetBool(groupName, "coordinatorEnabled", true);
    int sendWorkerIntervalMS = m_config->GetInt(groupName, "coordinatorPortMS", 15000);
    config.m_sendWorkerInterval = TimePoint(sendWorkerIntervalMS / 1000.);
}

void ConfiguredApplication::ReadLoggingConfig()
{
    m_loggerConfig.m_maxLogLevel = m_config->GetInt(m_defaultGroupName, "logLevel", m_loggerConfig.m_maxLogLevel);

    if (m_config->GetBool(m_defaultGroupName, "logToFile"))
    {
        m_loggerConfig.m_logType = LoggerConfig::LogType::Files;
        m_loggerConfig.m_dir = m_config->GetString(m_defaultGroupName, "logDir");
        if (m_loggerConfig.m_dir.empty())
            m_loggerConfig.m_dir = Application::Instance().GetAppDataDir() + "/Logs";
    }

    if (m_config->GetBool(m_defaultGroupName, "logToSyslog"))
        m_loggerConfig.m_logType = LoggerConfig::LogType::Syslog;

}

void ConfiguredApplication::ReadCompilerConfig()
{
    const std::string defaultGroup("compiler");

    m_compilerConfig.m_toolIds = m_config->GetStringList(defaultGroup, "modules");
    for (const auto & id : m_compilerConfig.m_toolIds)
    {
        CompilerConfig::CompilerUnit unit;
        unit.m_id = id;
        std::string append = m_config->GetString(defaultGroup, id + "_append");
        if (!append.empty())
        {
            unit.m_appendOption = " " + append + " ";
        }
        std::string type = m_config->GetString(defaultGroup, id + "_type", "gcc"); // "gcc" or "msvc"
        if (type == "msvc" || unit.m_id.find("ms") == 0)
            unit.m_type = CompilerConfig::ToolchainType::MSVC;
        for (auto executable: m_config->GetStringList(defaultGroup, id))
        {
#ifdef _WIN32
   std::replace(executable.begin(), executable.end(), '\\', '/');
#endif
            unit.m_names.push_back(executable);
        }

        m_compilerConfig.m_modules.push_back(unit);
    }
}

void ConfiguredApplication::ReadRemoteToolClientConfig()
{
    const std::string defaultGroup("client");
    m_remoteToolClientConfig.m_invocationAttempts = m_config->GetInt(defaultGroup, "invocationAttempts", m_remoteToolClientConfig.m_invocationAttempts);
    m_remoteToolClientConfig.m_minimalRemoteTasks = m_config->GetInt(defaultGroup, "minimalRemoteTasks", m_remoteToolClientConfig.m_minimalRemoteTasks);
    int queueTimeoutMS = m_config->GetInt(defaultGroup, "queueTimeoutMS");
    if (queueTimeoutMS)
        m_remoteToolClientConfig.m_queueTimeout = TimePoint(queueTimeoutMS / 1000.);

    ReadCoordinatorClientConfig(m_remoteToolClientConfig.m_coordinator, defaultGroup);
}

void ConfiguredApplication::ReadRemoteToolServerConfig()
{
    const std::string defaultGroup("worker");
    m_remoteToolServerConfig.m_listenPort   = m_config->GetInt(defaultGroup, "listenPort");
    m_remoteToolServerConfig.m_listenHost   = m_config->GetString(defaultGroup, "listenHost");
    m_remoteToolServerConfig.m_workersCount = m_config->GetInt(defaultGroup, "workersCount");
    m_remoteToolServerConfig.m_workerId     = m_config->GetString(defaultGroup, "workerId");
    ReadCoordinatorClientConfig(m_remoteToolServerConfig.m_coordinator, defaultGroup);
}

void ConfiguredApplication::ReadCoordinatorServerConfig()
{
    const std::string defaultGroup("coordinator");
    m_coordinatorServerConfig.m_listenPort = m_config->GetInt(defaultGroup, "listenPort");
}

void ConfiguredApplication::ReadCompilerProxyServerConfig()
{
    const std::string defaultGroup("proxy");
    m_compilerProxyServerConfig.m_listenPort = m_config->GetInt(defaultGroup, "listenPort");
    m_compilerProxyServerConfig.m_toolId = m_config->GetString(defaultGroup, "toolId");
}

}
