/*
 * Copyright (C) 2017 Smirnov Vladimir mapron1@gmail.com
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

#include "ToolInvocation.h"

#include <CommonTypes.h>

namespace Wuild {
/// Abstract command line parser for tool invocation
class ICommandLineParser {
public:
    using Ptr = std::shared_ptr<ICommandLineParser>;

public:
    virtual ~ICommandLineParser() = default;

    virtual ToolInvocation GetToolInvocation() const                           = 0;
    virtual void           SetToolInvocation(const ToolInvocation& invocation) = 0;

    ToolInvocation ProcessToolInvocation(const ToolInvocation& invocation)
    {
        SetToolInvocation(invocation);
        return GetToolInvocation();
    }

    virtual void SetInvokeType(ToolInvocation::InvokeType type) = 0;

    virtual bool IsRemotePossible() const = 0;

    virtual void RemoveLocalFlags()       = 0;
    virtual void RemoveDependencyFiles()  = 0;
    virtual void RemovePrepocessorFlags() = 0;
};
}
