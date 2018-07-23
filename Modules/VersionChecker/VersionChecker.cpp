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

#include "VersionChecker.h"

#include <FileUtils.h>

#include <regex>

namespace Wuild
{

VersionChecker::VersionChecker(ILocalExecutor::Ptr localExecutor)
	: m_localExecutor(std::move(localExecutor))
{

}

IVersionChecker::ToolType VersionChecker::GuessToolType(const ToolInvocation::Id &toolId) const
{
	if (toolId.m_toolExecutable.empty())
		throw std::logic_error("Tool id should be resolved before use.");

	const std::string executableName = FileInfo(toolId.m_toolExecutable).GetFullname();

	if (executableName.find("cl.exe") != std::string::npos)
		return ToolType::MSVC;

	if (executableName.find("clang") != std::string::npos)
		return ToolType::Clang;

	static const std::vector<std::string> s_gccNames {"gcc", "g++", "mingw"};
	for (const auto & gccName : s_gccNames)
	{
		if (executableName.find(gccName) != std::string::npos)
			return ToolType::GCC;
	}

	return ToolType::Unknown;
}

IVersionChecker::Version VersionChecker::GetToolVersion(const ToolInvocation::Id &toolId, IVersionChecker::ToolType type) const
{
	if (type == ToolType::Unknown)
		return IVersionChecker::Version();

	static const std::regex versionRegex("\\d+\\.[0-9.]+");

	auto versionCheckTask = std::make_shared<LocalExecutorTask>();
	versionCheckTask->m_readOutput = versionCheckTask->m_writeInput = false;
	versionCheckTask->m_invocation.m_id = toolId;
	IVersionChecker::Version result;
	if (type == ToolType::Clang || type == ToolType::GCC)
	{
		versionCheckTask->m_callback = [&result](LocalExecutorResult::Ptr taskResult){
			std::smatch match;
			if (std::regex_search(taskResult->m_stdOut, match, versionRegex))
				result = match[0].str();
		};
	}
	if (type == ToolType::Clang)
	{
		versionCheckTask->m_invocation.m_args = {"--version"};
		m_localExecutor->SyncExecTask(versionCheckTask);
	}
	else if (type == ToolType::GCC)
	{
		versionCheckTask->m_invocation.m_args = {"-dumpfullversion"};
		m_localExecutor->SyncExecTask(versionCheckTask);
	}

	return result;
}

IVersionChecker::VersionMap VersionChecker::DetermineToolVersions(IInvocationRewriter::Ptr rewriter) const
{
	const std::vector<std::string> & toolIds = rewriter->GetConfig().m_toolIds;
	VersionMap result;
	for (const auto & toolId : toolIds)
	{
		ToolInvocation::Id id;
		id.m_toolId = toolId;
		id = rewriter->CompleteToolId(id);
		if (id.m_toolExecutable.empty())
		{
			result[toolId] = "";
			continue;
		}

		const auto toolType = GuessToolType(id);
		const auto version = GetToolVersion(id, toolType);
		result[toolId] = version;
	}
	return result;
}

}
