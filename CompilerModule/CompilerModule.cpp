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

#include "CompilerModule.h"

#include "GccCommandLineParser.h"
#include "MsvcCommandLineParser.h"

#include <CommonTypes.h>
#include <FileUtils.h>
#include <StringUtils.h>
#include <algorithm>

namespace Wuild
{

InvocationRewriter::InvocationRewriter()
{
}

void InvocationRewriter::SetConfig(const IInvocationRewriter::Config &config)
{
	m_config = config;
}

const IInvocationRewriter::Config &InvocationRewriter::GetConfig() const
{
	return m_config;
}

bool InvocationRewriter::SplitInvocation(const ToolInvocation & original, ToolInvocation &preprocessor, ToolInvocation &compilation)
{
	ToolInfo toolInfo = CompileInfoById(original.m_id);
	if (!toolInfo.m_parser)
		return false;
	ToolInvocation origComplete = CompleteInvocation(original);

	if (origComplete.m_type != ToolInvocation::InvokeType::Compile)
		return false;

	toolInfo.m_parser->SetToolInvocation(origComplete);
	toolInfo.m_parser->SetInvokeType(ToolInvocation::InvokeType::Preprocess);
	toolInfo.m_parser->RemovePDB();
	preprocessor = toolInfo.m_parser->GetToolInvocation();

	toolInfo.m_parser->SetToolInvocation(origComplete);
	toolInfo.m_parser->RemovePDB();
	toolInfo.m_parser->RemovePrepocessorFlags();
	toolInfo.m_parser->RemoveDependencyFiles();
	compilation = toolInfo.m_parser->GetToolInvocation();

	const std::string srcFilename = origComplete.GetInput(); // we hope  .cpp is coming after -c flag. It's naive.
	const std::string objFilename = origComplete.GetOutput();
	std::string preprocessedFilename = objFilename;
	bool outIsVar = preprocessedFilename[0] == '$'; // ninja hack...
	if (!outIsVar)
	{
		preprocessedFilename = GetPreprocessedPath(srcFilename, objFilename);
		preprocessor.SetOutput(preprocessedFilename);
		compilation.SetInput(preprocessedFilename);
	}

	if (!toolInfo.m_append.empty())
		compilation.m_args.push_back(toolInfo.m_append);

	return true;
}

ToolInvocation InvocationRewriter::CompleteInvocation(const ToolInvocation &original)
{
	ToolInvocation inv;
	inv.m_args.clear();
	inv.m_ignoredArgs = original.m_ignoredArgs;
	for (auto arg : original.m_args)
	{
		StringVector argSplit;
		StringUtils::SplitString(StringUtils::Trim(arg), argSplit, ' ', false, true); //TODO: paths with spaces not supported!
		inv.m_args.insert(inv.m_args.end(), argSplit.begin(), argSplit.end());
	}

	ToolInfo info = CompileInfoById(original.m_id);
	if (info.m_valid)
	{
		inv.m_id = info.m_id;
		inv.m_type = original.m_type;
		inv = info.m_parser->ProcessToolInvocation(inv);
	}
	return inv;
}

ToolInvocation InvocationRewriter::FilterFlags(const ToolInvocation &original)
{
	ToolInfo info = CompileInfoById(original.m_id);
	if (info.m_valid)
	{
		 ToolInvocation flags = CompleteInvocation(original);
		 if (flags.m_type == ToolInvocation::InvokeType::Preprocess)
		 {
			info.m_parser->SetToolInvocation(flags);
			info.m_parser->RemovePDB();
			return info.m_parser->GetToolInvocation();
		 }
		 else if (flags.m_type == ToolInvocation::InvokeType::Compile)
		 {
			info.m_parser->SetToolInvocation(flags);
			info.m_parser->RemovePrepocessorFlags();
			info.m_parser->RemoveDependencyFiles();
			info.m_parser->RemovePDB();
			return info.m_parser->GetToolInvocation();
		 }
	}
	return original;
}

std::string InvocationRewriter::GetPreprocessedPath(const std::string & sourcePath,
												const std::string & objectPath) const
{
	FileInfo sourceInfo(sourcePath);
	FileInfo objectInfo(objectPath);

	const std::string preprocessed = objectInfo.GetDir(true) + "pp_" + objectInfo.GetNameWE() + sourceInfo.GetFullExtension();
	return preprocessed;
}

InvocationRewriter::ToolInfo InvocationRewriter::CompileInfoById(const ToolInvocation::Id &id) const
{
	if (id.m_toolId.empty())
		return CompileInfoByExecutable(id.m_toolExecutable);

	return CompileInfoByToolId(id.m_toolId);
}

InvocationRewriter::ToolInfo InvocationRewriter::CompileInfoByExecutable(const std::string &executable) const
{
   std::string exec = executable;
#ifdef _WIN32
   std::replace(exec.begin(), exec.end(), '\\', '/');
#endif

	InvocationRewriter::ToolInfo info;
	for (const Config::Tool & unit : m_config.m_tools)
	{
		if (std::find(unit.m_names.cbegin(), unit.m_names.cend(), exec) != unit.m_names.cend())
		{
			return CompileInfoByUnit(unit);
		}
	}
	return info;
}

InvocationRewriter::ToolInfo InvocationRewriter::CompileInfoByToolId(const std::string &toolId) const
{
	InvocationRewriter::ToolInfo info;
	for (const Config::Tool & unit : m_config.m_tools)
	{
		if (unit.m_id == toolId)
			return CompileInfoByUnit(unit);
	}
	return info;
}

InvocationRewriter::ToolInfo InvocationRewriter::CompileInfoByUnit(const IInvocationRewriter::Config::Tool &unit) const
{
	ToolInfo info;
	info.m_append = unit.m_appendOption;
	info.m_id.m_toolId = unit.m_id;
	info.m_id.m_toolExecutable = unit.m_names[0];
	if (unit.m_type == Config::ToolchainType::GCC)
		info.m_parser.reset(new GccCommandLineParser());
	else if (unit.m_type == Config::ToolchainType::MSVC)
		info.m_parser.reset(new MsvcCommandLineParser());
	if (info.m_parser)
		info.m_valid = true;
	return info;
}


}
