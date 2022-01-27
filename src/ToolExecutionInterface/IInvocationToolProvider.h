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

#include "IInvocationTool.h"

namespace Wuild {

class IInvocationToolProvider {
public:
    using Config = InvocationToolConfig;
    using Ptr    = std::shared_ptr<IInvocationToolProvider>;

public:
    virtual ~IInvocationToolProvider() = default;

    virtual const IInvocationTool::List& GetTools() const = 0;

    virtual const StringVector& GetToolIds() const = 0;

    virtual IInvocationTool::Ptr GetTool(const ToolId& id) const = 0;

    /// Performs substitution of toolId executable if possible.
    virtual ToolId CompleteToolId(const ToolId& original) const = 0;

    /// Checks if an invocation is a compilation (not nessesarily for tool in config)
    virtual bool IsCompilerInvocation(const ToolCommandline& original) const = 0;
};

}
