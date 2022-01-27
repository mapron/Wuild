/*
 * Copyright (C) 2017-2022 Smirnov Vladimir mapron1@gmail.com
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

#include "InvocationToolProvider.h"

#include "InvocationTool.h"
#include "GccCommandLineParser.h"
#include "MsvcCommandLineParser.h"

#include <Syslogger.h>

#include <algorithm>
#include <cassert>

namespace Wuild {

InvocationToolProvider::InvocationToolProvider(Config config)
    : m_config(std::move(config))
{
    for (const auto& toolConfig : m_config.m_tools) {
        if (toolConfig.m_type == Config::Tool::ToolchainType::AutoDetect)
            continue;
        auto         tool = std::make_shared<InvocationTool>(toolConfig);
        const size_t i    = m_tools.size();
        m_tools.push_back(tool);
        m_indexById[toolConfig.m_id] = i;
        if (!toolConfig.m_remoteAlias.empty())
            m_indexById[toolConfig.m_remoteAlias] = i;
        for (const auto& name : toolConfig.m_names) {
            if (!name.empty())
                m_indexByName[name] = i;
        }
        m_toolIds.push_back(toolConfig.m_id);
    }
}

IInvocationToolProvider::Ptr InvocationToolProvider::Create(Config config)
{
    auto guessType = [](const std::string& executableName) -> Config::Tool::ToolchainType {
        if (executableName.find("cl.exe") != std::string::npos)
            return Config::Tool::ToolchainType::MSVC;

        if (executableName.find("clang") != std::string::npos)
            return Config::Tool::ToolchainType::Clang;

        static const std::vector<std::string> s_gccNames{ "gcc", "g++", "mingw", "g__~1" }; // "g__~1" is Windows short name.
        for (const auto& gccName : s_gccNames) {
            if (executableName.find(gccName) != std::string::npos)
                return Config::Tool::ToolchainType::GCC;
        }
        return Config::Tool::ToolchainType::AutoDetect;
    };

    for (auto& tool : config.m_tools) {
        if (tool.m_type != Config::Tool::ToolchainType::AutoDetect)
            continue;

        const std::string executableName = FileInfo(tool.m_names[0]).GetFullname();
        tool.m_type                      = guessType(executableName);
        if (tool.m_type != Config::Tool::ToolchainType::AutoDetect)
            continue;

        if (tool.m_id.find("ms") != std::string::npos)
            tool.m_type = Config::Tool::ToolchainType::MSVC;
        else
            Syslogger(Syslogger::Warning) << "Failed to detect toolchain type for:" << tool.m_id;
    }

    auto res = std::make_shared<InvocationToolProvider>(std::move(config));
    return res;
}

const StringVector& InvocationToolProvider::GetToolIds() const
{
    return m_toolIds;
}

const IInvocationTool::List& InvocationToolProvider::GetTools() const
{
    return m_tools;
}

IInvocationTool::Ptr InvocationToolProvider::GetTool(const ToolId& id) const
{
    if (id.m_toolId.empty())
        return CompileInfoByExecutable(id.m_toolExecutable);

    return CompileInfoByToolId(id.m_toolId);
}

ToolId InvocationToolProvider::CompleteToolId(const ToolId& original) const
{
    auto tool = GetTool(original);
    return tool ? tool->GetId() : original;
}

bool InvocationToolProvider::IsCompilerInvocation(const ToolCommandline& original) const
{
    // we DO NOT look into our m_tools, because this can be compiler invocation which is unconfigured.
    // so we can possible tell user we have configuration problem (usage of a compiler that not configured as a tool).
    GccCommandLineParser  gcc;
    MsvcCommandLineParser msvc;

    ToolCommandline inv = original;
    inv.m_type          = ToolCommandline::InvokeType::Unknown;

    auto checker = [&inv](ICommandLineParser& parser) -> bool {
        auto parsedInv = parser.ProcessToolInvocation(inv);
        return parsedInv.m_type == ToolCommandline::InvokeType::Compile;
    };
    return checker(gcc) || checker(msvc);
}

IInvocationTool::Ptr InvocationToolProvider::CompileInfoByExecutable(const std::string& executable) const
{
    const std::string exec = FileInfo::ToPlatformPath(FileInfo::LocatePath(executable));
    auto              it   = m_indexByName.find(exec);
    if (it == m_indexByName.cend())
        return nullptr;

    return m_tools[it->second];
}

IInvocationTool::Ptr InvocationToolProvider::CompileInfoByToolId(const std::string& toolId) const
{
    auto it = m_indexById.find(toolId);
    if (it == m_indexById.cend())
        return nullptr;

    return m_tools[it->second];
}

}
