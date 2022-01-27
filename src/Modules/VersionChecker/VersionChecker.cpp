/*
 * Copyright (C) 2018-2021 Smirnov Vladimir mapron1@gmail.com
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

#include "VersionChecker.h"

#include <FileUtils.h>
#include <Syslogger.h>

#include <regex>

namespace Wuild {

VersionChecker::VersionChecker(ILocalExecutor::Ptr localExecutor, IInvocationToolProvider::Ptr invocationToolProvider)
    : m_localExecutor(std::move(localExecutor))
    , m_invocationToolProvider(std::move(invocationToolProvider))
{
}

IVersionChecker::VersionMap VersionChecker::DetermineToolVersions(const std::vector<std::string>& toolIds) const
{
    VersionMap result;
    for (auto&& tool : m_invocationToolProvider->GetTools()) {
        ToolId id = tool->GetId();
        if (!toolIds.empty() && std::find(toolIds.cbegin(), toolIds.cend(), id.m_toolId) == toolIds.cend())
            continue;

        if (!tool->GetConfig().m_version.empty()) {
            result[id.m_toolId] = tool->GetConfig().m_version;
            continue;
        }
        if (id.m_toolExecutable.empty()) {
            result[id.m_toolId] = "";
            continue;
        }

        const auto version  = GetToolVersion(id, tool->GetConfig().m_type);
        result[id.m_toolId] = version;
    }
    return result;
}

IVersionChecker::Version VersionChecker::GetToolVersion(const ToolId& toolId, ToolType type) const
{
    if (type == ToolType::AutoDetect || type == ToolType::UpdateFile)
        return IVersionChecker::Version();

    static const std::regex versionRegexGnu("\\d+\\.[0-9.]+");
    static const std::regex versionRegexCl(R"(\d+\.\d+\.\d+(\.\d+)? for \w+)");

    auto versionCheckTask          = std::make_shared<LocalExecutorTask>();
    versionCheckTask->m_readOutput = versionCheckTask->m_writeInput = false;
    versionCheckTask->m_invocation.m_id                             = toolId;
    versionCheckTask->m_readStderr                                  = false;

    IVersionChecker::Version result;

    versionCheckTask->m_callback = [&result, type](LocalExecutorResult::Ptr taskResult) {
        std::smatch match;
        if (std::regex_search(taskResult->m_stdOut, match, type == ToolType::MSVC ? versionRegexCl : versionRegexGnu)) {
            result = match[0].str();
        }
    };

    if (type == ToolType::Clang)
        versionCheckTask->m_invocation.m_arglist.m_args = { "--version" };
    else if (type == ToolType::GCC)
        versionCheckTask->m_invocation.m_arglist.m_args = { "-dumpfullversion", "-dumpversion" };

    m_localExecutor->SyncExecTask(versionCheckTask);

    return result;
}

}
