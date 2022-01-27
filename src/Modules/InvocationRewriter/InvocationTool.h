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
#pragma once

#include "ICommandLineParser.h"

#include <IInvocationTool.h>

namespace Wuild {

class InvocationTool : public IInvocationTool {
public:
    struct ToolInfo {
        ICommandLineParser::Ptr m_parser;
        ToolId                  m_id;
        Config                  m_tool;
        std::string             m_remoteId;
    };

public:
    InvocationTool(const Config& toolConfig);

    ToolId GetId() const override;

    const Config& GetConfig() const override;

    bool SplitInvocation(const ToolCommandline& original,
                         ToolCommandline&       preprocessor,
                         ToolCommandline&       compilation,
                         std::string*           remoteToolId = nullptr) const override;

    ToolCommandline CompleteInvocation(const ToolCommandline& original) const override;

    bool            CheckRemotePossibleForFlags(const ToolCommandline& original) const override;
    ToolCommandline FilterFlags(const ToolCommandline& original) const override;
    ToolCommandline PrepareRemote(const ToolCommandline& original) const override;

private:
    ToolInfo m_toolInfo;
};

}
