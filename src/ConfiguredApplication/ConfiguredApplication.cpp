/*
 * Copyright (C) 2017-2021 Smirnov Vladimir mapron1@gmail.com
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
#include "ArgStorage.h"

#include <Application.h>
#include <StringUtils.h>
#include <Syslogger.h>
#include <FileUtils.h>

#include <utility>

#include <memory>

namespace {
const std::string g_defaultConfigSubfolder = ".Wuild/";
const std::string g_defaultConfig          = "Wuild.ini";
const std::string g_envConfig              = "WUILD_CONFIG";

Wuild::RemoteToolClientConfig::PostProcess ParsePostProcess(const std::string& str)
{
    using namespace Wuild;
    if (str.empty())
        return {};

    StringVector parts;
    StringUtils::SplitString(str, parts, ',', true, true);
    if (parts.empty())
        return {};
    const size_t itemCount = parts.size();
    if (itemCount % 2 != 0) {
        Syslogger(Syslogger::Err) << "postProcess parts count must be even";
        return {};
    }
    RemoteToolClientConfig::PostProcess result;
    for (size_t i = 0; i < itemCount; i += 2) {
        const auto& needle      = parts[i];
        const auto& replacement = parts[i + 1];
        const auto  size        = needle.size();
        if (size != replacement.size()) {
            Syslogger(Syslogger::Err) << "postProcess pairs must be equal";
            return {};
        }
        ByteArray needleBa(size), replacementBa(size);
        std::transform(needle.cbegin(), needle.cend(), needleBa.begin(), [](char c) { return static_cast<uint8_t>(c); });
        std::transform(replacement.cbegin(), replacement.cend(), replacementBa.begin(), [](char c) { return static_cast<uint8_t>(c); });
        result.m_items.push_back({ std::move(needleBa), std::move(replacementBa) });
    }
    return result;
}
}

namespace Wuild {

ConfiguredApplication::ConfiguredApplication(const StringVector& argConfig, const std::string& appName, const std::string& defaultGroupName)
    : m_defaultGroupName(std::move(defaultGroupName))
{
    if (!appName.empty())
        Application::Instance().SetAppName(appName);

    m_config               = std::make_unique<AbstractConfig>();
    AbstractConfig& config = *m_config;

    config.ReadCommandLine(argConfig);
    std::string configPath = config.GetString("", "config");
    if (configPath.empty()) {
        char* envPath = getenv(g_envConfig.c_str());
        if (envPath)
            configPath = envPath;
    }
    bool unexistendConfigIsError = true;

    if (configPath.empty()) {
        unexistendConfigIsError = false;
        const std::vector<std::string> configPaths{
            Application::Instance().GetHomeDir() + "/" + g_defaultConfigSubfolder + g_defaultConfig,
#ifdef _WIN32
            Application::Instance().GetHomeDir() + "/" + g_defaultConfig,
#endif
            Application::Instance().GetExecutablePath() + g_defaultConfig,
        };
        for (const auto& path : configPaths)
            if (FileInfo(path).Exists()) {
                configPath = path;
                break;
            }
    }

    if (!config.ReadIniFile(configPath) && unexistendConfigIsError) {
        Syslogger(Syslogger::Err) << "Failed to load:" << configPath;
    }
    // if global variable is set in inifile, override in again with commandline
    config.ReadCommandLine(argConfig);

    ReadLoggingConfig();
    ReadInvocationToolProviderConfig();
    ReadRemoteToolClientConfig();
    ReadRemoteToolServerConfig();
    ReadCoordinatorServerConfig();
    ReadToolProxyServerConfig();

    InitLogging(m_loggerConfig);

    Syslogger() << "Using Wuild config = " << configPath;

    m_tempDir = m_config->GetString(m_defaultGroupName, "tempDir", Application::Instance().GetTempDir());
}

ConfiguredApplication::ConfiguredApplication(int argc, char** argv, const std::string& appName, const std::string& defaultGroupName)
    : ConfiguredApplication(ArgStorage(argc, argv).GetConfigValues(), appName, defaultGroupName)
{
}

ConfiguredApplication::~ConfiguredApplication() = default;

bool ConfiguredApplication::InitLogging(const LoggerConfig& loggerConfig)
{
    std::ostringstream os;
    if (loggerConfig.Validate(&os)) {
        std::unique_ptr<ILoggerBackend> backend;
        switch (loggerConfig.m_logType) {
            case LoggerConfig::LogType::Syslog:
#ifdef __linux__
                backend = std::make_unique<LoggerBackendSyslog>(loggerConfig.m_maxLogLevel,
                                                                loggerConfig.m_outputLoglevel,
                                                                loggerConfig.m_outputTimestamp,
                                                                loggerConfig.m_outputTimeoffsets);
                break;
#endif
                //fallthrough
            case LoggerConfig::LogType::Cout:
            case LoggerConfig::LogType::Cerr:
            case LoggerConfig::LogType::Printf:
                backend = std::make_unique<LoggerBackendConsole>(loggerConfig.m_maxLogLevel,
                                                                 loggerConfig.m_outputLoglevel,
                                                                 loggerConfig.m_outputTimestamp,
                                                                 loggerConfig.m_outputTimeoffsets,
                                                                 loggerConfig.m_logType == LoggerConfig::LogType::Printf ? LoggerBackendConsole::Type::Printf : (loggerConfig.m_logType == LoggerConfig::LogType::Cerr ? LoggerBackendConsole::Type::Cerr : LoggerBackendConsole::Type::Cout));
                break;
            case LoggerConfig::LogType::Files:
                backend = std::make_unique<LoggerBackendFiles>(loggerConfig.m_maxLogLevel,
                                                               loggerConfig.m_outputLoglevel,
                                                               loggerConfig.m_outputTimestamp,
                                                               loggerConfig.m_outputTimeoffsets,
                                                               loggerConfig.m_maxFilesInDir,
                                                               loggerConfig.m_maxMessagesInFile,
                                                               loggerConfig.m_dir);
                break;
            default:
                break;
        }
        if (backend)
            Syslogger::SetLoggerBackend(std::move(backend));
    } else {
        Syslogger(Syslogger::Err) << os.str();
    }
    return true;
}

bool ConfiguredApplication::GetInvocationToolConfig(InvocationToolConfig& config, bool silent) const
{
    // TODO: make template, remove copy-paste.
    std::ostringstream os;
    if (!m_invocationToolProviderConfig.Validate(&os)) {
        if (!silent)
            Syslogger(Syslogger::Err) << os.str();
        return false;
    }
    config = m_invocationToolProviderConfig;
    return true;
}

bool ConfiguredApplication::GetCoordinatorServerConfig(CoordinatorServerConfig& config) const
{
    // TODO: make template, remove copy-paste.
    std::ostringstream os;
    if (!m_coordinatorServerConfig.Validate(&os)) {
        Syslogger(Syslogger::Err) << os.str();
        return false;
    }
    config = m_coordinatorServerConfig;
    return true;
}

bool ConfiguredApplication::GetToolProxyServerConfig(ToolProxyServerConfig& config) const
{
    std::ostringstream os;
    if (!m_toolProxyServerConfig.Validate(&os)) {
        Syslogger(Syslogger::Err) << os.str();
        return false;
    }
    config = m_toolProxyServerConfig;
    return true;
}

bool ConfiguredApplication::GetRemoteToolClientConfig(RemoteToolClientConfig& config, bool silent) const
{
    std::ostringstream os;
    if (!m_remoteToolClientConfig.Validate(&os)) {
        if (!silent)
            Syslogger(Syslogger::Err) << os.str();
        return false;
    }
    config = m_remoteToolClientConfig;
    return true;
}

bool ConfiguredApplication::GetRemoteToolServerConfig(RemoteToolServerConfig& config, bool silent) const
{
    std::ostringstream os;
    if (!m_remoteToolServerConfig.Validate(&os)) {
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

void ConfiguredApplication::ReadCoordinatorClientConfig(CoordinatorClientConfig& config, const std::string& groupName)
{
    config.m_coordinatorPort  = m_config->GetInt(groupName, "coordinatorPort");
    config.m_coordinatorHost  = m_config->GetStringList(groupName, "coordinatorHost");
    config.m_enabled          = m_config->GetBool(groupName, "coordinatorEnabled", true);
    int sendInfoIntervalMS    = m_config->GetInt(groupName, "sendInfoIntervalMS", 15000);
    config.m_sendInfoInterval = TimePoint(sendInfoIntervalMS / 1000.);
}

void ConfiguredApplication::ReadLoggingConfig()
{
    m_loggerConfig.m_maxLogLevel       = m_config->GetInt(m_defaultGroupName, "logLevel", m_loggerConfig.m_maxLogLevel);
    m_loggerConfig.m_maxMessagesInFile = m_config->GetInt(m_defaultGroupName, "maxMessagesInFile", m_loggerConfig.m_maxMessagesInFile);
    m_loggerConfig.m_maxFilesInDir     = m_config->GetInt(m_defaultGroupName, "maxFilesInDir", m_loggerConfig.m_maxFilesInDir);
    m_loggerConfig.m_outputTimestamp   = m_config->GetBool(m_defaultGroupName, "outputTimestamp", m_loggerConfig.m_outputTimestamp);
    m_loggerConfig.m_outputTimeoffsets = m_config->GetBool(m_defaultGroupName, "outputTimeoffsets", m_loggerConfig.m_outputTimeoffsets);
    m_loggerConfig.m_outputLoglevel    = m_config->GetBool(m_defaultGroupName, "outputLoglevel", m_loggerConfig.m_outputLoglevel);

    if (m_config->GetBool(m_defaultGroupName, "logToFile")) {
        m_loggerConfig.m_logType = LoggerConfig::LogType::Files;
        m_loggerConfig.m_dir     = m_config->GetString(m_defaultGroupName, "logDir");
        if (m_loggerConfig.m_dir.empty())
            m_loggerConfig.m_dir = Application::Instance().GetAppDataDir() + "/Logs";
    }

    if (m_config->GetBool(m_defaultGroupName, "logToSyslog"))
        m_loggerConfig.m_logType = LoggerConfig::LogType::Syslog;
    else if (m_config->GetBool(m_defaultGroupName, "logToCerr"))
        m_loggerConfig.m_logType = LoggerConfig::LogType::Cerr;
}

void ConfiguredApplication::ReadInvocationToolProviderConfig()
{
    const std::string defaultGroup("tools");
    const bool        disableVersionChecks = m_config->GetBool(defaultGroup, "disableVersionChecks");
    if (disableVersionChecks)
        Syslogger(Syslogger::Warning) << "Warning: compiler version checks disabled!";

    m_invocationToolProviderConfig.m_toolIds = m_config->GetStringList(defaultGroup, "toolIds");
    for (const auto& id : m_invocationToolProviderConfig.m_toolIds) {
        InvocationToolConfig::Tool unit;
        unit.m_id          = id;
        std::string append = m_config->GetString(defaultGroup, id + "_appendRemote");
        if (!append.empty()) {
            unit.m_appendRemote = " " + append + " ";
        }
        unit.m_removeRemote = m_config->GetString(defaultGroup, id + "_removeRemote");
        unit.m_remoteAlias  = m_config->GetString(defaultGroup, id + "_remoteAlias");
        unit.m_version      = m_config->GetString(defaultGroup, id + "_version");

        if (disableVersionChecks)
            unit.m_version = InvocationToolConfig::VERSION_NO_CHECK;

        std::string type = m_config->GetString(defaultGroup, id + "_type", "auto"); // "gcc"|"clang"|"msvc"
        if (type == "msvc")
            unit.m_type = InvocationToolConfig::Tool::ToolchainType::MSVC;
        else if (type == "gcc")
            unit.m_type = InvocationToolConfig::Tool::ToolchainType::GCC;
        else if (type == "clang")
            unit.m_type = InvocationToolConfig::Tool::ToolchainType::Clang;
        else if (type == "update_file")
            unit.m_type = InvocationToolConfig::Tool::ToolchainType::UpdateFile;

        for (auto executable : m_config->GetStringList(defaultGroup, id)) {
            executable                  = FileInfo::ToPlatformPath(executable);
            const std::string shortName = FileInfo(executable).GetPlatformShortName();
            if (shortName != executable)
                unit.m_names.push_back(shortName);
            unit.m_names.push_back(executable);
        }

        m_invocationToolProviderConfig.m_tools.push_back(unit);
    }
}

void ConfiguredApplication::ReadRemoteToolClientConfig()
{
    const std::string defaultGroup("toolClient");
    m_remoteToolClientConfig.m_invocationAttempts = m_config->GetInt(defaultGroup, "invocationAttempts", m_remoteToolClientConfig.m_invocationAttempts);
    m_remoteToolClientConfig.m_minimalRemoteTasks = m_config->GetInt(defaultGroup, "minimalRemoteTasks", m_remoteToolClientConfig.m_minimalRemoteTasks);
    m_remoteToolClientConfig.m_maxLoadAverage     = m_config->GetDouble(defaultGroup, "maxLoadAverage", m_remoteToolClientConfig.m_maxLoadAverage);
    m_remoteToolClientConfig.m_postProcess        = ParsePostProcess(m_config->GetString(defaultGroup, "postProcess"));

    int queueTimeoutMS = m_config->GetInt(defaultGroup, "queueTimeoutMS");
    if (queueTimeoutMS)
        m_remoteToolClientConfig.m_queueTimeout = TimePoint(queueTimeoutMS / 1000.);

    int requestTimeoutMS = m_config->GetInt(defaultGroup, "requestTimeoutMS");
    if (requestTimeoutMS)
        m_remoteToolClientConfig.m_requestTimeout = TimePoint(requestTimeoutMS / 1000.);

    ReadCoordinatorClientConfig(m_remoteToolClientConfig.m_coordinator, defaultGroup);
    m_remoteToolClientConfig.m_coordinator.m_redundance = CoordinatorClientConfig::Redundance::Any;
    ReadCompressionConfig(m_remoteToolClientConfig.m_compression, defaultGroup);

    auto& toolServers     = m_remoteToolClientConfig.m_initialToolServers;
    toolServers.m_hosts   = m_config->GetStringList(defaultGroup, "toolserverHosts");
    toolServers.m_port    = m_config->GetInt(defaultGroup, "toolserverPort");
    toolServers.m_toolIds = m_config->GetStringList(defaultGroup, "toolserverIds");
}

void ConfiguredApplication::ReadRemoteToolServerConfig()
{
    const std::string defaultGroup("toolServer");
    m_remoteToolServerConfig.m_listenPort           = m_config->GetInt(defaultGroup, "listenPort");
    m_remoteToolServerConfig.m_listenHost           = m_config->GetString(defaultGroup, "listenHost");
    m_remoteToolServerConfig.m_threadCount          = m_config->GetInt(defaultGroup, "threadCount", m_remoteToolServerConfig.m_threadCount);
    m_remoteToolServerConfig.m_serverName           = m_config->GetString(defaultGroup, "serverName");
    m_remoteToolServerConfig.m_hostsWhiteList       = m_config->GetStringList(defaultGroup, "hostsWhiteList");
    m_remoteToolServerConfig.m_useClientCompression = m_config->GetBool(defaultGroup, "useClientCompression", m_remoteToolServerConfig.m_useClientCompression);
    ReadCoordinatorClientConfig(m_remoteToolServerConfig.m_coordinator, defaultGroup);
    ReadCompressionConfig(m_remoteToolServerConfig.m_compression, defaultGroup);
}

void ConfiguredApplication::ReadCoordinatorServerConfig()
{
    const std::string defaultGroup("coordinator");
    m_coordinatorServerConfig.m_listenPort = m_config->GetInt(defaultGroup, "listenPort");
}

void ConfiguredApplication::ReadCompressionConfig(CompressionInfo& compressionInfo, const std::string& groupName)
{
    auto type = m_config->GetString(groupName, "compressionType", "ZStd");
    if (type == "None")
        compressionInfo.m_type = CompressionType::None;
    else if (type == "ZStd")
        compressionInfo.m_type = CompressionType::ZStd;
    else
        Syslogger(Syslogger::Err) << "Invalid compression type:" << type;
    compressionInfo.m_level = m_config->GetInt(groupName, "compressionLevel", 3);
}

void ConfiguredApplication::ReadToolProxyServerConfig()
{
    const std::string defaultGroup("proxy");
    m_toolProxyServerConfig.m_listenPort   = m_config->GetInt(defaultGroup, "listenPort");
    m_toolProxyServerConfig.m_toolId       = m_config->GetString(defaultGroup, "toolId");
    m_toolProxyServerConfig.m_startCommand = m_config->GetString(defaultGroup, "startCommand", Application::Instance().GetExecutablePath() + "WuildProxy");
    m_toolProxyServerConfig.m_threadCount  = m_config->GetInt(defaultGroup, "threadCount", m_toolProxyServerConfig.m_threadCount);

    int proxyClientTimeoutMS = m_config->GetInt(defaultGroup, "proxyClientTimeoutMS");
    if (proxyClientTimeoutMS)
        m_toolProxyServerConfig.m_proxyClientTimeout = TimePoint(proxyClientTimeoutMS / 1000.);
    int inactiveTimeoutMS = m_config->GetInt(defaultGroup, "inactiveTimeoutMS");
    if (inactiveTimeoutMS)
        m_toolProxyServerConfig.m_inactiveTimeout = TimePoint(inactiveTimeoutMS / 1000.);
}

}
