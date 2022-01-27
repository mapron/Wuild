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

#include <IInvocationToolProvider.h>

#include <unordered_map>

namespace Wuild {

class InvocationToolProvider : public IInvocationToolProvider {
public:
    InvocationToolProvider(Config config);
    static IInvocationToolProvider::Ptr Create(Config config);

    const StringVector&          GetToolIds() const override;
    const IInvocationTool::List& GetTools() const override;
    IInvocationTool::Ptr         GetTool(const ToolId& id) const override;

    ToolId CompleteToolId(const ToolId& original) const override;

    bool IsCompilerInvocation(const ToolCommandline& original) const override;

private:
    IInvocationTool::Ptr CompileInfoByExecutable(const std::string& executable) const;
    IInvocationTool::Ptr CompileInfoByToolId(const std::string& toolId) const;

private:
    const Config                            m_config;
    StringVector                            m_toolIds;
    IInvocationTool::List                   m_tools;
    std::unordered_map<std::string, size_t> m_indexByName;
    std::unordered_map<std::string, size_t> m_indexById;
};

}
