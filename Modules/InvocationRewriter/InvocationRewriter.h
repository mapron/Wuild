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

#include "IInvocationRewriter.h"
#include "ICommandLineParser.h"

namespace Wuild
{

class InvocationRewriter : public IInvocationRewriter
{
	InvocationRewriter();
public:

	static IInvocationRewriter::Ptr Create(const IInvocationRewriter::Config & config)
	{
		auto res = IInvocationRewriter::Ptr(new InvocationRewriter());
		res->SetConfig(config);
		return res;
	}

	void SetConfig(const Config & config) override;
	const Config& GetConfig() const override;

	bool IsCompilerInvocation(const ToolInvocation & original) const override;

	bool SplitInvocation(const ToolInvocation & original,
						 ToolInvocation & preprocessor,
						 ToolInvocation & compilation,
						 std::string * remoteToolId = nullptr) const override;

   ToolInvocation CompleteInvocation(const ToolInvocation & original) const override;

   ToolInvocation::Id CompleteToolId(const ToolInvocation::Id & original) const override;

   bool CheckRemotePossibleForFlags(const ToolInvocation & original) const override;
   ToolInvocation FilterFlags(const ToolInvocation & original) const override;

   std::string GetPreprocessedPath(const std::string & sourcePath, const std::string & objectPath) const override;

   ToolInvocation PrepareRemote(const ToolInvocation & original) const override;


private:
	struct ToolInfo
	{
		ICommandLineParser::Ptr m_parser;
		ToolInvocation::Id m_id;
		Config::Tool m_tool;
		std::string m_remoteId;
		bool m_valid = false;
	};
	ToolInfo CompileInfoById(const ToolInvocation::Id & id) const;
	ToolInfo CompileInfoByExecutable(const std::string & executable) const;
	ToolInfo CompileInfoByToolId(const std::string & toolId) const;
	ToolInfo CompileInfoByUnit(const Config::Tool & unit) const;

	Config m_config;
};

}
