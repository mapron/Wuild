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
#include "InvocationTool.h"

#include "GccCommandLineParser.h"
#include "MsvcCommandLineParser.h"
#include "UpdateFileCommandParser.h"

#include <cassert>

namespace Wuild {

InvocationTool::InvocationTool(const Config& toolConfig)
{
    m_toolInfo.m_tool                = toolConfig;
    m_toolInfo.m_id.m_toolId         = toolConfig.m_id;
    m_toolInfo.m_id.m_toolExecutable = toolConfig.m_names[0];
    if (toolConfig.m_type == Config::ToolchainType::GCC || toolConfig.m_type == Config::ToolchainType::Clang)
        m_toolInfo.m_parser.reset(new GccCommandLineParser());
    else if (toolConfig.m_type == Config::ToolchainType::MSVC)
        m_toolInfo.m_parser.reset(new MsvcCommandLineParser());
    else if (toolConfig.m_type == Config::ToolchainType::UpdateFile)
        m_toolInfo.m_parser.reset(new UpdateFileCommandParser());
    else
        assert(!"Invalid logic");

    m_toolInfo.m_remoteId = m_toolInfo.m_id.m_toolId;
    if (!toolConfig.m_remoteAlias.empty())
        m_toolInfo.m_remoteId = toolConfig.m_remoteAlias;
}

ToolId InvocationTool::GetId() const
{
    return m_toolInfo.m_id;
}

const IInvocationTool::Config& InvocationTool::GetConfig() const
{
    return m_toolInfo.m_tool;
}

bool InvocationTool::SplitInvocation(const ToolCommandline& original, ToolCommandline& preprocessor, ToolCommandline& compilation, std::string* remoteToolId) const
{
    ToolCommandline origComplete = CompleteInvocation(original);

    if (origComplete.m_type != ToolCommandline::InvokeType::Compile)
        return false;

    if (remoteToolId)
        *remoteToolId = m_toolInfo.m_remoteId;

    {
        ICommandLineParser::Options opt;
        opt.m_changeType       = ToolCommandline::InvokeType::Preprocess;
        opt.m_removeLocalFlags = true; // @todo: investigate why it's needed.
        auto result            = m_toolInfo.m_parser->Process(origComplete, opt);
        if (!result.m_success)
            return false;

        preprocessor = result.m_inv;
    }

    {
        ICommandLineParser::Options opt;
        opt.m_removeLocalFlags       = true;
        opt.m_removeDependencyFiles  = true;
        opt.m_removePrepocessorFlags = true;
        auto result                  = m_toolInfo.m_parser->Process(origComplete, opt);
        if (!result.m_success)
            return false;

        compilation = result.m_inv;
    }

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

ToolCommandline InvocationTool::CompleteInvocation(const ToolCommandline& original) const
{
    ToolCommandline inv = original;
    inv.m_id            = m_toolInfo.m_id; // original may have only one part of id - executable ot toolId.
    auto result         = m_toolInfo.m_parser->Process(inv, {});
    inv                 = result.m_inv;
    return inv;
}

bool InvocationTool::CheckRemotePossibleForFlags(const ToolCommandline& original) const
{
    auto result = m_toolInfo.m_parser->Process(original, {});
    return result.m_isRemotePossible;
}

ToolCommandline InvocationTool::FilterFlags(const ToolCommandline& original) const
{
    if (original.m_type == ToolCommandline::InvokeType::Preprocess) {
        ICommandLineParser::Options opt;
        opt.m_removeLocalFlags = true;
        auto result            = m_toolInfo.m_parser->Process(original, opt);
        return result.m_inv;
    }
    if (original.m_type == ToolCommandline::InvokeType::Compile) {
        ICommandLineParser::Options opt;
        opt.m_removeLocalFlags       = true;
        opt.m_removeDependencyFiles  = true;
        opt.m_removePrepocessorFlags = true;
        auto result                  = m_toolInfo.m_parser->Process(original, opt);
        return result.m_inv;
    }

    return original;
}

ToolCommandline InvocationTool::PrepareRemote(const ToolCommandline& original) const
{
    ToolCommandline inv = CompleteInvocation(original);

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

    // strip filenames directories.
    inv.SetInput(FileInfo(inv.GetInput()).GetFullname());
    inv.SetOutput(FileInfo(inv.GetOutput()).GetFullname());
    return inv;
}

}
