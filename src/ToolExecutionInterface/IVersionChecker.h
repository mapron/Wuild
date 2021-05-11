/*
 * Copyright (C) 2018 Smirnov Vladimir mapron1@gmail.com
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

#include <map>

namespace Wuild {
/// Interface for retrieving version information for tool.
class IVersionChecker {
public:
    using Ptr        = std::shared_ptr<IVersionChecker>;
    using Version    = std::string;
    using VersionMap = std::map<std::string, Version>; //!< toolId=>Version

    enum class ToolType
    {
        Unknown,
        GCC,
        Clang,
        MSVC
    };

public:
    virtual ~IVersionChecker() = default;

    /// Guess tool type by executable name.
    virtual ToolType GuessToolType(const ToolInvocation::Id& toolId) const = 0;

    /// For each id in toolIds, determine version using GetToolVersion and place key in map.
    virtual VersionMap DetermineToolVersions(const std::vector<std::string>& toolIds) const = 0;
};

}
