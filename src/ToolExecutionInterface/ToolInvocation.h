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

#include "ArgumentList.h"

namespace Wuild {
/// Represents tool invocation line (executable with argumnents). Allows changing input/output filenames in arguments.
class ToolInvocation {
public:
    /// Type of invocation.
    enum class InvokeType
    {
        Unknown,
        Preprocess,
        Compile
    };
    struct Id {
        std::string m_toolId;         //!< abstract toolchain id (configurable)
        std::string m_toolExecutable; //!< full tool executable path
    };

public:
    ToolInvocation(StringVector args = StringVector(), InvokeType type = InvokeType::Unknown);
    ToolInvocation(const std::string& args, InvokeType type);

    void FetchExecutableFromArgs();

    void        SetArgsString(const std::string& args);
    std::string GetArgsString() const;

    bool        SetInput(const std::string& filename);
    std::string GetInput() const;

    bool        SetOutput(const std::string& filename);
    std::string GetOutput() const;

    ToolInvocation& SetId(const std::string& toolId);
    ToolInvocation& SetExecutable(const std::string& toolExecutable);

public:
    Id           m_id;
    InvokeType   m_type = InvokeType::Unknown;
    ArgumentList m_arglist;
    StringVector m_ignoredArgs;
    int          m_inputNameIndex  = -1;
    int          m_outputNameIndex = -1;
};
}
