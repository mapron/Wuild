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

#include "ICompilerModule.h"
#include "ICommandLineParser.h"

namespace Wuild
{

class CompilerModule : public ICompilerModule
{
    CompilerModule();
public:

    static ICompilerModule::Ptr Create(const ICompilerModule::Config & config)
    {
        auto res = ICompilerModule::Ptr(new CompilerModule());
        res->SetConfig(config);
        return res;
    }

    void SetConfig(const Config & config) override;
    const Config& GetConfig() const override;

    bool SplitInvocation(const CompilerInvocation & original,
                         CompilerInvocation & preprocessor,
                         CompilerInvocation & compilation) override;

   CompilerInvocation CompleteInvocation(const CompilerInvocation & original) override;

   CompilerInvocation FilterFlags(const CompilerInvocation & original) override;

   std::string GetPreprocessedPath(const std::string & sourcePath, const std::string & objectPath) const override;


private:
    struct CompileInfo
    {
        ICommandLineParser::Ptr m_parser;
        CompilerInvocation::Id m_id;
        std::string m_append;
        bool m_valid = false;
    };
    CompileInfo CompileInfoById(const CompilerInvocation::Id & id) const;
    CompileInfo CompileInfoByExecutable(const std::string & executable) const;
    CompileInfo CompileInfoByCompilerId(const std::string & compilerId) const;
    CompileInfo CompileInfoByUnit(const Config::CompilerUnit & unit) const;

    Config m_config;
};

}
