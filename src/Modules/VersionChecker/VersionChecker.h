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

#pragma once

#include <IVersionChecker.h>
#include <IInvocationToolProvider.h>

#include <ILocalExecutor.h>

namespace Wuild {
class VersionChecker : public IVersionChecker {
    VersionChecker(ILocalExecutor::Ptr localExecutor, IInvocationToolProvider::Ptr invocationToolProvider);

public:
    static Ptr Create(ILocalExecutor::Ptr localExecutor, IInvocationToolProvider::Ptr invocationToolProvider)
    {
        return Ptr(new VersionChecker(std::move(localExecutor), std::move(invocationToolProvider)));
    }

    VersionMap DetermineToolVersions(const std::vector<std::string>& toolIds) const override;

private:
    using ToolType = IInvocationTool::Config::ToolchainType;
    Version GetToolVersion(const ToolId& toolId, ToolType type) const;

private:
    ILocalExecutor::Ptr          m_localExecutor;
    IInvocationToolProvider::Ptr m_invocationToolProvider;
};

}
