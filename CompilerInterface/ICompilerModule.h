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

#include "CompilerInvocation.h"

#include <CommonTypes.h>
#include <StringUtils.h>
#include <CompilerConfig.h>

#include <sstream>
namespace Wuild
{
/// Abstract toolchain invocation manager.
///
/// Can determine compiler info from invocataion; moreover, split invocation to preprocess and compilation.
class ICompilerModule
{
public:
    using Config = CompilerConfig;
    using StringPair = std::pair<std::string, std::string>;
    using Ptr = std::shared_ptr<ICompilerModule>;

public:
    virtual ~ICompilerModule() = default;

    virtual void SetConfig(const Config & config)  = 0;
    virtual const Config& GetConfig() const = 0;

    /// Split invocation on two steps. If succeeded, returns true.
    virtual bool SplitInvocation(const CompilerInvocation & original,
                                 CompilerInvocation & preprocessor,
                                 CompilerInvocation & compilation) = 0;

    /// Normalizes invocation struct, making possible to replace input/output files. Substitute toolId if possible.
    virtual CompilerInvocation CompleteInvocation(const CompilerInvocation & original) = 0;

    /// Remove preprocessor flags from splitted compilation phase; also remove extra preprocessor flags which not supported for distributed build.
    virtual CompilerInvocation FilterFlags(const CompilerInvocation & original) = 0;

    /// Get preprocessed filename path.
    virtual std::string GetPreprocessedPath(const std::string & sourcePath, const std::string & objectPath) const = 0;

};

}
