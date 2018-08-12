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
#include <Syslogger.h>

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

	static const std::vector<std::string> s_gccNames {"gcc", "g++", "mingw", "g__~1"}; // "g__~1" is Windows short name.
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

	static const std::regex versionRegexGnu("\\d+\\.[0-9.]+");
	static const std::regex versionRegexCl(R"(\d+\.\d+\.\d+\.\d+ for \w+)");

	auto versionCheckTask = std::make_shared<LocalExecutorTask>();
	versionCheckTask->m_readOutput = versionCheckTask->m_writeInput = false;
	versionCheckTask->m_invocation.m_id = toolId;
	IVersionChecker::Version result;

	versionCheckTask->m_callback = [&result, type](LocalExecutorResult::Ptr taskResult){
		std::smatch match;
		// Syslogger(Syslogger::Warning) << taskResult->m_stdOut << " matching '" << (type == ToolType::MSVC ? R"(\d+\.\d+\.\d+\.\d+ for \w+)" : "\\d+\\.[0-9.]+") << "'";
		if (std::regex_search(taskResult->m_stdOut, match, type == ToolType::MSVC ? versionRegexCl : versionRegexGnu))
		{
			result = match[0].str();
			Syslogger(Syslogger::Warning) << "match=" << result;
		}
	};
	
	if (type == ToolType::Clang)
		versionCheckTask->m_invocation.m_args = {"--version"};
	else if (type == ToolType::GCC)
		versionCheckTask->m_invocation.m_args = {"-dumpfullversion", "-dumpversion"};

	m_localExecutor->SyncExecTask(versionCheckTask);

	return result;
}

IVersionChecker::VersionMap VersionChecker::DetermineToolVersions(IInvocationRewriter::Ptr rewriter, const std::vector<std::string> & toolIds) const
{
	VersionMap result;
	for (const InvocationRewriterConfig::Tool & tool : rewriter->GetConfig().m_tools)
	{
		if (std::find(toolIds.cbegin(), toolIds.cend(), tool.m_id) == toolIds.cend())
			continue;

		if (!tool.m_version.empty())
		{
			result[tool.m_id] = tool.m_version;
			continue;
		}
		ToolInvocation::Id id;
		id.m_toolId = tool.m_id;
		id = rewriter->CompleteToolId(id);
		if (id.m_toolExecutable.empty())
		{
			Syslogger(Syslogger::Warning) << tool.m_id << "->''";
			result[tool.m_id] = "";
			continue;
		}

		const auto toolType = GuessToolType(id);
		const auto version = GetToolVersion(id, toolType);
		Syslogger(Syslogger::Warning) << tool.m_id << "->" << version;
		result[tool.m_id] = version;
	}
	return result;
}

}
