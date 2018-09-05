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

#include "InvocationRewriter.h"

#include "GccCommandLineParser.h"
#include "MsvcCommandLineParser.h"
#include "UpdateFileCommandParser.h"

#include <CommonTypes.h>
#include <FileUtils.h>
#include <StringUtils.h>
#include <Syslogger.h>

#include <algorithm>

namespace Wuild
{

namespace 
{

bool IsSpace(char c) { return c == ' '  || c == '\t'; }
bool IsQuote(char c) { return c == '\"' || c == '\''; }

// not a real cmd parser funstion; it just skips quoted strings, doing no unescape.
StringVector SplitShellCommand(const std::string & str)
{
	StringVector ret;
#ifndef _WIN32
	bool escape = false;
#endif
	std::string buffer;
	buffer.reserve(str.size());
	bool inQuoted = false;
	
	for (char c : str)
	{
	#ifndef _WIN32
		if (escape)
		{
			buffer += c;
			escape = false;
			continue;
		}

		if (c == '\\')
		{
			buffer += c;
			escape = true;
			continue;
		}
	#endif
		
		if (IsQuote(c))
		{
			inQuoted = !inQuoted;
		}
		else if (IsSpace(c) && !inQuoted)
		{
			if (!buffer.empty())
				ret.emplace_back(buffer);
			
			buffer.clear();
			continue;
		}
		
		// neither quote nor space
		buffer += c;
	}
	
	if (!buffer.empty())
		ret.emplace_back(buffer);
	
	return ret;
}
}

InvocationRewriter::InvocationRewriter() = default;

void InvocationRewriter::SetConfig(const IInvocationRewriter::Config &config)
{
	m_config = config;
}

const IInvocationRewriter::Config &InvocationRewriter::GetConfig() const
{
	return m_config;
}

bool InvocationRewriter::SplitInvocation(const ToolInvocation & original, ToolInvocation &preprocessor, ToolInvocation &compilation, std::string * remoteToolId)
{
	ToolInfo toolInfo = CompileInfoById(original.m_id);
	if (!toolInfo.m_parser)
		return false;
	ToolInvocation origComplete = CompleteInvocation(original);

	if (origComplete.m_type != ToolInvocation::InvokeType::Compile)
		return false;

	if (remoteToolId)
		*remoteToolId = toolInfo.m_remoteId;
	toolInfo.m_parser->SetToolInvocation(origComplete);
	toolInfo.m_parser->SetInvokeType(ToolInvocation::InvokeType::Preprocess);
	toolInfo.m_parser->RemoveLocalFlags();
	preprocessor = toolInfo.m_parser->GetToolInvocation();

	toolInfo.m_parser->SetToolInvocation(origComplete);
	toolInfo.m_parser->RemoveLocalFlags();
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

	return true;
}

ToolInvocation InvocationRewriter::CompleteInvocation(const ToolInvocation &original) const
{
	ToolInvocation inv = original;
	inv.m_type = ToolInvocation::InvokeType::Unknown;
	inv.m_args.clear();
	for (const auto& arg : original.m_args)
	{
		StringVector argSplit = SplitShellCommand(arg);
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

ToolInvocation::Id InvocationRewriter::CompleteToolId(const ToolInvocation::Id &original) const
{
	ToolInfo info = CompileInfoById(original);
	if (info.m_valid)
		return info.m_id;
	
	return original;
}

bool InvocationRewriter::CheckRemotePossibleForFlags(const ToolInvocation & original) const
{
	ToolInfo info = CompileInfoById(original.m_id);
	if (info.m_valid)
	{
		ToolInvocation flags = CompleteInvocation(original);
		info.m_parser->SetToolInvocation(flags);
		return info.m_parser->IsRemotePossible();
	}
	return false;
}

ToolInvocation InvocationRewriter::FilterFlags(const ToolInvocation &original) const
{
	ToolInfo info = CompileInfoById(original.m_id);
	if (info.m_valid)
	{
		 ToolInvocation flags = CompleteInvocation(original);
		 if (flags.m_type == ToolInvocation::InvokeType::Preprocess)
		 {
			info.m_parser->SetToolInvocation(flags);
			info.m_parser->RemoveLocalFlags();
			return info.m_parser->GetToolInvocation();
		 }
		 if (flags.m_type == ToolInvocation::InvokeType::Compile)
		 {
			info.m_parser->SetToolInvocation(flags);
			info.m_parser->RemovePrepocessorFlags();
			info.m_parser->RemoveDependencyFiles();
			info.m_parser->RemoveLocalFlags();
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

ToolInvocation InvocationRewriter::PrepareRemote(const ToolInvocation &original) const
{
	ToolInvocation inv = CompleteInvocation(original);
	ToolInfo info = CompileInfoById(original.m_id);
	if (info.m_valid)
	{
		inv.m_id = info.m_id;
		inv.m_type = original.m_type;
		inv = info.m_parser->ProcessToolInvocation(inv);
		if (!info.m_tool.m_appendRemote.empty())
			inv.m_args.push_back(info.m_tool.m_appendRemote);

		if (!info.m_tool.m_removeRemote.empty())
		{
			StringVector newArgs;
			for (const auto & arg : inv.m_args)
				if (arg != info.m_tool.m_removeRemote)
					newArgs.push_back(arg);
			newArgs.swap(inv.m_args);
		}
		inv.m_id.m_toolId = info.m_remoteId;
	}
	inv.SetInput (FileInfo(inv.GetInput() ).GetFullname());
	inv.SetOutput(FileInfo(inv.GetOutput()).GetFullname());
	return inv;
}

InvocationRewriter::ToolInfo InvocationRewriter::CompileInfoById(const ToolInvocation::Id &id) const
{
	if (id.m_toolId.empty())
		return CompileInfoByExecutable(id.m_toolExecutable);

	return CompileInfoByToolId(id.m_toolId);
}

InvocationRewriter::ToolInfo InvocationRewriter::CompileInfoByExecutable(const std::string &executable) const
{
	const std::string exec = FileInfo::ToPlatformPath(FileInfo::LocatePath(executable));
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
	if (toolId.empty())
		return info;

	for (const Config::Tool & unit : m_config.m_tools)
	{
		if (unit.m_id == toolId)
			return CompileInfoByUnit(unit);
	}
	for (const Config::Tool & unit : m_config.m_tools)
	{
		if (unit.m_remoteAlias == toolId)
			return CompileInfoByUnit(unit);
	}
	return info;
}

InvocationRewriter::ToolInfo InvocationRewriter::CompileInfoByUnit(const IInvocationRewriter::Config::Tool &unit) const
{
	ToolInfo info;
	info.m_tool = unit;
	info.m_id.m_toolId = unit.m_id;
	info.m_id.m_toolExecutable = unit.m_names[0];
	if (unit.m_type == Config::ToolchainType::GCC)
		info.m_parser.reset(new GccCommandLineParser());
	else if (unit.m_type == Config::ToolchainType::MSVC)
		info.m_parser.reset(new MsvcCommandLineParser());
	else if (unit.m_type == Config::ToolchainType::UpdateFile)
		info.m_parser.reset(new UpdateFileCommandParser());

	if (info.m_parser)
		info.m_valid = true;
	info.m_remoteId = info.m_id.m_toolId;
	if (!unit.m_remoteAlias.empty())
	{
		info.m_remoteId = unit.m_remoteAlias;
	}
	return info;
}


}
