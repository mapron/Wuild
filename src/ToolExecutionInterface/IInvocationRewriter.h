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

#include "ToolInvocation.h"

#include <CommonTypes.h>
#include <StringUtils.h>
#include <InvocationRewriterConfig.h>

#include <sstream>
namespace Wuild {
/// Abstract toolchain invocation manager.
///
/// Can determine tool info from invocataion; moreover, split invocation to preprocess and compilation.
class IInvocationRewriter {
public:
    using Config     = InvocationRewriterConfig;
    using StringPair = std::pair<std::string, std::string>;
    using Ptr        = std::shared_ptr<IInvocationRewriter>;

public:
    virtual ~IInvocationRewriter() = default;

    virtual const Config& GetConfig() const               = 0;

    /// Checks if an invocation is a compilation
    virtual bool IsCompilerInvocation(const ToolInvocation& original) const = 0;

    /// Split invocation on two steps. If succeeded, returns true.
    virtual bool SplitInvocation(const ToolInvocation& original,
                                 ToolInvocation&       preprocessor,
                                 ToolInvocation&       compilation,
                                 std::string*          remoteToolId = nullptr) const = 0;

    /// Normalizes invocation struct, making possible to replace input/output files. Substitute toolId if possible.
    virtual ToolInvocation CompleteInvocation(const ToolInvocation& original) const = 0;

    /// Performs substitution of toolId executable if possible.
    virtual ToolInvocation::Id CompleteToolId(const ToolInvocation::Id& original) const = 0;

    virtual bool CheckRemotePossibleForFlags(const ToolInvocation& original) const = 0;

    /// Remove preprocessor flags from splitted compilation phase; also remove extra preprocessor flags which not supported for distributed build.
    virtual ToolInvocation FilterFlags(const ToolInvocation& original) const = 0;

    /// Get preprocessed filename path.
    virtual std::string GetPreprocessedPath(const std::string& sourcePath, const std::string& objectPath) const = 0;

    /// Prepare invocation for remote execution
    virtual ToolInvocation PrepareRemote(const ToolInvocation& original) const = 0;
};

}
