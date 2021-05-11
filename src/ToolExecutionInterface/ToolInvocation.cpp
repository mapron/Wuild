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

#include "ToolInvocation.h"

#include <StringUtils.h>

#include <utility>
#include <cassert>

namespace Wuild
{
ToolInvocation::ToolInvocation(StringVector args, InvokeType type)
	: m_type(type)
	, m_args(std::move(args))
{

}

ToolInvocation::ToolInvocation(const std::string &args, ToolInvocation::InvokeType type)
	: m_type(type)
{
	SetArgsString(args);
}

void ToolInvocation::ParseArgsAsCommanline()
{
	assert(m_id.m_toolExecutable.empty());
	assert(!m_args.empty());
	StringVector args;
	std::string current;
	bool startedQuote = false;
	auto consume = [&current, &args](bool force) {
		if (force || !current.empty())
			args.push_back(current);
		current.clear();
	};
	for (const auto & arg : m_args) {
		for (char c : arg) {
			if (startedQuote){
				if (c == '"') {
					startedQuote = false;
					consume(true);
				} else {
					current += c;
				}
			}else{
				if (c == '"')
					startedQuote = true;
				else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') 
					consume(false);
				else
					current += c;
			}
		}
	}
	consume(false);
	assert(!args.empty());
	SetExecutable(args[0]);
	args.erase(args.begin());
	m_args = args;
}

void ToolInvocation::SetArgsString(const std::string &args)
{
	m_args.resize(1);
	m_args[0] = args;
}

std::string ToolInvocation::GetArgsString(bool prependExecutable) const
{
	std::string ret;
	if (prependExecutable)
		ret += m_id.m_toolExecutable + " ";
	return ret + StringUtils::JoinString( m_args, ' ');
}

bool ToolInvocation::SetInput(const std::string &filename)
{
	if (m_inputNameIndex < 0)
		return false;

	m_args[m_inputNameIndex] = filename;
	return true;
}

std::string ToolInvocation::GetInput() const
{
	if (m_inputNameIndex < 0)
		return std::string();

	return m_args[m_inputNameIndex];
}

bool ToolInvocation::SetOutput(const std::string &filename)
{
	if (m_outputNameIndex < 0)
		return false;

	m_args[m_outputNameIndex] = filename;
	return true;
}

std::string ToolInvocation::GetOutput() const
{
	if (m_outputNameIndex < 0)
		return std::string();

	return m_args[m_outputNameIndex];
}

ToolInvocation &ToolInvocation::SetId(const std::string &toolId)
{
	m_id.m_toolId = toolId;
	return *this;
}

ToolInvocation &ToolInvocation::SetExecutable(const std::string &toolExecutable)
{
	m_id.m_toolExecutable = toolExecutable;
	return *this;
}

}
