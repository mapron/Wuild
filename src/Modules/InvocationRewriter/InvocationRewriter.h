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

#include "IInvocationRewriter.h"
#include "ICommandLineParser.h"

#include <unordered_map>

namespace Wuild {

class InvocationRewriter : public IInvocationRewriterProvider {
public:
    class InvocationRewriterTool : public IInvocationRewriter {
    public:
        struct ToolInfo {
            ICommandLineParser::Ptr m_parser;
            ToolInvocation::Id      m_id;
            Config                  m_tool;
            std::string             m_remoteId;
        };

    public:
        InvocationRewriterTool(const Config& toolConfig);

        ToolInvocation::Id GetId() const override;

        const Config& GetConfig() const override;

        bool SplitInvocation(const ToolInvocation& original,
                             ToolInvocation&       preprocessor,
                             ToolInvocation&       compilation,
                             std::string*          remoteToolId = nullptr) const override;

        ToolInvocation CompleteInvocation(const ToolInvocation& original) const override;

        bool           CheckRemotePossibleForFlags(const ToolInvocation& original) const override;
        ToolInvocation FilterFlags(const ToolInvocation& original) const override;

        ToolInvocation PrepareRemote(const ToolInvocation& original) const override;

    private:
        ToolInfo m_toolInfo;
    };

    InvocationRewriter(IInvocationRewriterProvider::Config config);
    static IInvocationRewriterProvider::Ptr Create(IInvocationRewriterProvider::Config config);

    const StringVector& GetToolIds() const override;

    const IInvocationRewriter::List& GetTools() const override;

    IInvocationRewriter::Ptr GetTool(const ToolInvocation::Id& id) const override;

    bool IsCompilerInvocation(const ToolInvocation& original) const override;

    ToolInvocation::Id CompleteToolId(const ToolInvocation::Id& original) const override;

private:
    IInvocationRewriter::Ptr CompileInfoByExecutable(const std::string& executable) const;
    IInvocationRewriter::Ptr CompileInfoByToolId(const std::string& toolId) const;

private:
    const Config                            m_config;
    StringVector                            m_toolIds;
    IInvocationRewriter::List               m_tools;
    std::unordered_map<std::string, size_t> m_indexByName;
    std::unordered_map<std::string, size_t> m_indexById;
};

}
