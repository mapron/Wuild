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

#include "InvocationRewriter.h"

#include "GccCommandLineParser.h"
#include "MsvcCommandLineParser.h"
#include "UpdateFileCommandParser.h"

#include <CommonTypes.h>
#include <FileUtils.h>
#include <StringUtils.h>
#include <Syslogger.h>
#include <ArgumentList.h>

#include <algorithm>
#include <cassert>

namespace Wuild {

InvocationRewriter::InvocationRewriter(Config config)
    : m_config(std::move(config))
{
    for (const auto& tool : m_config.m_tools) {
        if (tool.m_type == Config::ToolchainType::AutoDetect)
            continue;
        auto         toolProvider = std::make_shared<InvocationRewriterTool>(tool);
        const size_t i            = m_tools.size();
        m_tools.push_back(toolProvider);
        m_indexById[tool.m_id] = i;
        if (!tool.m_remoteAlias.empty())
            m_indexById[tool.m_remoteAlias] = i;
        for (const auto& name : tool.m_names) {
            if (!name.empty())
                m_indexByName[name] = i;
        }
        m_toolIds.push_back(tool.m_id);
    }
}

InvocationRewriter::InvocationRewriterTool::InvocationRewriterTool(const InvocationRewriterConfig::Tool& toolConfig)
{
    m_toolInfo.m_tool                = toolConfig;
    m_toolInfo.m_id.m_toolId         = toolConfig.m_id;
    m_toolInfo.m_id.m_toolExecutable = toolConfig.m_names[0];
    if (toolConfig.m_type == InvocationRewriterConfig::ToolchainType::GCC || toolConfig.m_type == InvocationRewriterConfig::ToolchainType::Clang)
        m_toolInfo.m_parser.reset(new GccCommandLineParser());
    else if (toolConfig.m_type == InvocationRewriterConfig::ToolchainType::MSVC)
        m_toolInfo.m_parser.reset(new MsvcCommandLineParser());
    else if (toolConfig.m_type == InvocationRewriterConfig::ToolchainType::UpdateFile)
        m_toolInfo.m_parser.reset(new UpdateFileCommandParser());
    else
        assert(!"Invalid logic");

    m_toolInfo.m_remoteId = m_toolInfo.m_id.m_toolId;
    if (!toolConfig.m_remoteAlias.empty())
        m_toolInfo.m_remoteId = toolConfig.m_remoteAlias;
}

ToolInvocation::Id InvocationRewriter::InvocationRewriterTool::GetId() const
{
    return m_toolInfo.m_id;
}

const IInvocationRewriter::Config& InvocationRewriter::InvocationRewriterTool::GetConfig() const
{
    return m_toolInfo.m_tool;
}

IInvocationRewriterProvider::Ptr InvocationRewriter::Create(Config config)
{
    auto guessType = [](const std::string& executableName) -> Config::ToolchainType {
        if (executableName.find("cl.exe") != std::string::npos)
            return Config::ToolchainType::MSVC;

        if (executableName.find("clang") != std::string::npos)
            return Config::ToolchainType::Clang;

        static const std::vector<std::string> s_gccNames{ "gcc", "g++", "mingw", "g__~1" }; // "g__~1" is Windows short name.
        for (const auto& gccName : s_gccNames) {
            if (executableName.find(gccName) != std::string::npos)
                return Config::ToolchainType::GCC;
        }
        return Config::ToolchainType::AutoDetect;
    };

    for (auto& tool : config.m_tools) {
        if (tool.m_type != Config::ToolchainType::AutoDetect)
            continue;

        const std::string executableName = FileInfo(tool.m_names[0]).GetFullname();
        tool.m_type                      = guessType(executableName);
        if (tool.m_type != Config::ToolchainType::AutoDetect)
            continue;

        if (tool.m_id.find("ms") != std::string::npos)
            tool.m_type = Config::ToolchainType::MSVC;
        else
            Syslogger(Syslogger::Warning) << "Failed to detect toolchain type for:" << tool.m_id;
    }

    auto res = std::make_shared<InvocationRewriter>(std::move(config));
    return res;
}

const StringVector& InvocationRewriter::GetToolIds() const
{
    return m_toolIds;
}

const IInvocationRewriter::List& InvocationRewriter::GetTools() const
{
    return m_tools;
}

bool InvocationRewriter::IsCompilerInvocation(const ToolInvocation& original) const
{
    // we DO NOT look into our m_tools, because this can be compiler invocation which is unconfigured.
    // so we can possible tell user we have configuration problem (usage of a compiler that not configured as a tool).
    GccCommandLineParser  gcc;
    MsvcCommandLineParser msvc;

    ToolInvocation inv = original;
    inv.m_type         = ToolInvocation::InvokeType::Unknown;

    auto checker = [&inv](ICommandLineParser& parser) -> bool {
        auto parsedInv = parser.ProcessToolInvocation(inv);
        return parsedInv.m_type == ToolInvocation::InvokeType::Compile;
    };
    return checker(gcc) || checker(msvc);
}

bool InvocationRewriter::InvocationRewriterTool::SplitInvocation(const ToolInvocation& original, ToolInvocation& preprocessor, ToolInvocation& compilation, std::string* remoteToolId) const
{
    ToolInvocation origComplete = CompleteInvocation(original);

    if (origComplete.m_type != ToolInvocation::InvokeType::Compile) {
        origComplete = CompleteInvocation(original);
        return false;
    }

    if (remoteToolId)
        *remoteToolId = m_toolInfo.m_remoteId;
    m_toolInfo.m_parser->SetToolInvocation(origComplete);
    m_toolInfo.m_parser->SetInvokeType(ToolInvocation::InvokeType::Preprocess);
    m_toolInfo.m_parser->RemoveLocalFlags();
    preprocessor = m_toolInfo.m_parser->GetToolInvocation();

    m_toolInfo.m_parser->SetToolInvocation(origComplete);
    m_toolInfo.m_parser->RemoveLocalFlags();
    m_toolInfo.m_parser->RemovePrepocessorFlags();
    m_toolInfo.m_parser->RemoveDependencyFiles();
    compilation = m_toolInfo.m_parser->GetToolInvocation();

    const std::string srcFilename          = origComplete.GetInput(); // we hope  .cpp is coming after -c flag. It's naive.
    const std::string objFilename          = origComplete.GetOutput();
    std::string       preprocessedFilename = objFilename;
    bool              outIsVar             = preprocessedFilename[0] == '$'; // ninja hack...
    if (!outIsVar) {
        preprocessedFilename = GetPreprocessedPath(srcFilename, objFilename);
        preprocessor.SetOutput(preprocessedFilename);
        compilation.SetInput(preprocessedFilename);
    }

    return true;
}

ToolInvocation InvocationRewriter::InvocationRewriterTool::CompleteInvocation(const ToolInvocation& original) const
{
    ToolInvocation inv = original;
    inv.m_id           = m_toolInfo.m_id; // original may have only one part of id - executable ot toolId.
    inv                = m_toolInfo.m_parser->ProcessToolInvocation(inv);
    return inv;
}

ToolInvocation::Id InvocationRewriter::CompleteToolId(const ToolInvocation::Id& original) const
{
    auto tool = GetTool(original);
    return tool ? tool->GetId() : original;
}

bool InvocationRewriter::InvocationRewriterTool::CheckRemotePossibleForFlags(const ToolInvocation& original) const
{
    ToolInvocation flags = CompleteInvocation(original);
    m_toolInfo.m_parser->SetToolInvocation(flags);
    return m_toolInfo.m_parser->IsRemotePossible();
}

ToolInvocation InvocationRewriter::InvocationRewriterTool::FilterFlags(const ToolInvocation& original) const
{
    ToolInvocation flags = CompleteInvocation(original);
    if (flags.m_type == ToolInvocation::InvokeType::Preprocess) {
        m_toolInfo.m_parser->SetToolInvocation(flags);
        m_toolInfo.m_parser->RemoveLocalFlags();
        return m_toolInfo.m_parser->GetToolInvocation();
    }
    if (flags.m_type == ToolInvocation::InvokeType::Compile) {
        m_toolInfo.m_parser->SetToolInvocation(flags);
        m_toolInfo.m_parser->RemovePrepocessorFlags();
        m_toolInfo.m_parser->RemoveDependencyFiles();
        m_toolInfo.m_parser->RemoveLocalFlags();
        return m_toolInfo.m_parser->GetToolInvocation();
    }

    return original;
}

/* // commented to make git diff history nicier.
std::string InvocationRewriter::GetPreprocessedPath(const std::string& sourcePath,
                                                    const std::string& objectPath) const
{
    FileInfo sourceInfo(sourcePath);
    FileInfo objectInfo(objectPath);

    const std::string preprocessed = objectInfo.GetDir(true) + "pp_" + objectInfo.GetNameWE() + sourceInfo.GetFullExtension();
    return preprocessed;
}
*/

ToolInvocation InvocationRewriter::InvocationRewriterTool::PrepareRemote(const ToolInvocation& original) const
{
    ToolInvocation inv = CompleteInvocation(original);

    if (!m_toolInfo.m_tool.m_appendRemote.empty())
        inv.m_arglist.m_args.push_back(m_toolInfo.m_tool.m_appendRemote);

    if (!m_toolInfo.m_tool.m_removeRemote.empty()) {
        StringVector newArgs;
        for (const auto& arg : inv.m_arglist.m_args)
            if (arg != m_toolInfo.m_tool.m_removeRemote)
                newArgs.push_back(arg);
        newArgs.swap(inv.m_arglist.m_args);
    }
    inv.m_id.m_toolId = m_toolInfo.m_remoteId;

    inv.SetInput(FileInfo(inv.GetInput()).GetFullname());
    inv.SetOutput(FileInfo(inv.GetOutput()).GetFullname());
    return inv;
}

IInvocationRewriter::Ptr InvocationRewriter::GetTool(const ToolInvocation::Id& id) const
{
    if (id.m_toolId.empty())
        return CompileInfoByExecutable(id.m_toolExecutable);

    return CompileInfoByToolId(id.m_toolId);
}

IInvocationRewriter::Ptr InvocationRewriter::CompileInfoByExecutable(const std::string& executable) const
{
    const std::string exec = FileInfo::ToPlatformPath(FileInfo::LocatePath(executable));
    auto              it   = m_indexByName.find(exec);
    if (it == m_indexByName.cend())
        return nullptr;

    return m_tools[it->second];
}

IInvocationRewriter::Ptr InvocationRewriter::CompileInfoByToolId(const std::string& toolId) const
{
    auto it = m_indexById.find(toolId);
    if (it == m_indexById.cend())
        return nullptr;

    return m_tools[it->second];
}

}
