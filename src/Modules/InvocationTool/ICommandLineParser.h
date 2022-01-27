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

#include "ToolCommandline.h"

#include <CommonTypes.h>

namespace Wuild {
/// Abstract command line parser for tool invocation
class ICommandLineParser {
public:
    using Ptr = std::shared_ptr<ICommandLineParser>;

public:
    virtual ~ICommandLineParser() = default;

    struct Result {
        bool            m_success = false;
        ToolCommandline m_inv;
        bool            m_isRemotePossible = true;
    };

    struct Options {
        ToolCommandline::InvokeType m_changeType             = ToolCommandline::InvokeType::Unknown;
        bool                        m_removeLocalFlags       = false;
        bool                        m_removeDependencyFiles  = false;
        bool                        m_removePrepocessorFlags = false;
    };

    virtual Result Process(const ToolCommandline& invocation, const Options& options) const = 0;
};
}
