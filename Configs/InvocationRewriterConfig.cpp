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

#include "InvocationRewriterConfig.h"

#include <iostream>
#include <sstream>

namespace Wuild
{

std::string InvocationRewriterConfig::GetFirstToolId() const
{
	return m_tools.empty() ? "" : m_tools[0].m_id;
}

std::string InvocationRewriterConfig::GetFirstToolName() const
{
	return m_tools.empty() || m_tools[0].m_names.empty() ? "" : m_tools[0].m_names[0];
}

bool InvocationRewriterConfig::Validate(std::ostream *errStream) const
{
	if (m_tools.empty())
	{
		if (errStream)
			*errStream << "Toolchain modules are empty.";
		return false;
	}
	for (const auto & unit : m_tools)
	{
		if (unit.m_names.empty())
		{
			if (errStream)
				*errStream << "Invalid config for " << unit.m_id;
			return false;
		}
	}
	return true;
}

std::string InvocationRewriterConfig::Dump() const
{
	std::ostringstream os;
	for (const Tool & tool : m_tools)
	{
		os << tool.m_id << ": ";
		for (const auto & name : tool.m_names)
			os << name << ", ";
		os << "\n";
	}
	return os.str();
}

}
