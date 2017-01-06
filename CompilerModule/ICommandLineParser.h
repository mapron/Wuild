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

namespace Wuild
{
/// Abstract command line parser for compiler toolchain.
class ICommandLineParser
{
public:
    using Ptr = std::shared_ptr<ICommandLineParser>;

public:
    virtual ~ICommandLineParser() = default;

    virtual CompilerInvocation GetCompilerInvocation() const = 0;
    virtual void SetCompilerInvocation(const CompilerInvocation & invocation) = 0;

    CompilerInvocation ProcessCompilerInvocation(const CompilerInvocation & invocation)
    {
        SetCompilerInvocation(invocation);
        return GetCompilerInvocation();
    }

    virtual void SetInvokeType(CompilerInvocation::InvokeType type) = 0;

    virtual void RemovePDB() = 0;
    virtual void RemoveDependencyFiles() = 0;
    virtual void RemovePrepocessorFlags() = 0;
};
}
