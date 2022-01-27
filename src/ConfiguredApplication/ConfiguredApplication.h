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

#pragma once

#include <CommonTypes.h>
#include <Syslogger.h>
#include <InvocationToolConfig.h>
#include <LoggerConfig.h>
#include <RemoteToolClientConfig.h>
#include <RemoteToolServerConfig.h>
#include <CoordinatorServerConfig.h>
#include <CoordinatorClientConfig.h>
#include <ToolProxyServerConfig.h>

#include <algorithm>

namespace Wuild {
class AbstractConfig;

/// Contains prepared configs for different services
class ConfiguredApplication {
public:
    LoggerConfig            m_loggerConfig;
    InvocationToolConfig    m_invocationToolProviderConfig;
    RemoteToolClientConfig  m_remoteToolClientConfig;
    RemoteToolServerConfig  m_remoteToolServerConfig;
    CoordinatorServerConfig m_coordinatorServerConfig;
    ToolProxyServerConfig   m_toolProxyServerConfig;

    std::string m_tempDir;

    /// parses parameter from commandline, reading wuild configuration, filling Config structures.
    ConfiguredApplication(const StringVector& argConfig, const std::string& appName = std::string(), const std::string& defaultGroupName = std::string());
    /// this function keeps argc and argv untouched; if you need to modification after parsing, use previous constructor with ArgStorage() call.
    ConfiguredApplication(int argc, char** argv, const std::string& appName = std::string(), const std::string& defaultGroupName = std::string());

    ~ConfiguredApplication();

    bool InitLogging(const LoggerConfig& loggerConfig);

    bool GetInvocationToolConfig(InvocationToolConfig& config, bool silent = false) const;
    bool GetCoordinatorServerConfig(CoordinatorServerConfig& config) const;
    bool GetToolProxyServerConfig(ToolProxyServerConfig& config) const;

    bool GetRemoteToolClientConfig(RemoteToolClientConfig& config, bool silent = false) const;
    bool GetRemoteToolServerConfig(RemoteToolServerConfig& config, bool silent = false) const;

    std::string DumpAllConfigValues() const;

private:
    ConfiguredApplication(const ConfiguredApplication&) = delete;

    void ReadLoggingConfig();
    void ReadInvocationToolProviderConfig();
    void ReadRemoteToolClientConfig();
    void ReadRemoteToolServerConfig();
    void ReadCoordinatorClientConfig(CoordinatorClientConfig& config, const std::string& groupName);
    void ReadCoordinatorServerConfig();
    void ReadCompressionConfig(CompressionInfo& compressionInfo, const std::string& groupName);
    void ReadToolProxyServerConfig();

private:
    std::unique_ptr<AbstractConfig> m_config;
    std::string                     m_defaultGroupName;
};
}
