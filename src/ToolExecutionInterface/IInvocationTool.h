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

#include "ToolCommandline.h"

#include <CommonTypes.h>
#include <StringUtils.h>
#include <InvocationToolConfig.h>
#include <FileUtils.h>

namespace Wuild {
/// Abstract toolchain invocation manager.
///
/// Can determine tool info from invocation; moreover, split invocation to preprocess and compilation.
class IInvocationTool {
public:
    using StringPair = std::pair<std::string, std::string>;
    using Ptr        = std::shared_ptr<IInvocationTool>;
    using List       = std::vector<Ptr>;
    using Config     = InvocationToolConfig::Tool;

public:
    virtual ~IInvocationTool() = default;

    virtual ToolId GetId() const = 0;

    virtual const Config& GetConfig() const = 0;

    /// Split invocation on two steps. If succeeded, returns true.
    virtual bool SplitInvocation(const ToolCommandline& original,
                                 ToolCommandline&       preprocessor,
                                 ToolCommandline&       compilation,
                                 std::string*           remoteToolId = nullptr) const = 0;

    /// Normalizes invocation struct, making possible to replace input/output files. Substitute toolId if possible.
    virtual ToolCommandline CompleteInvocation(const ToolCommandline& original) const = 0;

    virtual bool CheckRemotePossibleForFlags(const ToolCommandline& original) const = 0;

    /// Remove preprocessor flags from splitted compilation phase; also remove extra preprocessor flags which not supported for distributed build.
    virtual ToolCommandline FilterFlags(const ToolCommandline& original) const = 0;

    /// Prepare invocation for remote execution
    virtual ToolCommandline PrepareRemote(const ToolCommandline& original) const = 0;

    static std::string GetPreprocessedPath(const std::string& sourcePath,
                                           const std::string& objectPath)
    {
        FileInfo sourceInfo(sourcePath);
        FileInfo objectInfo(objectPath);

        const std::string preprocessed = objectInfo.GetDir(true) + "pp_" + objectInfo.GetNameWE() + sourceInfo.GetFullExtension();
        return preprocessed;
    }
};

}
