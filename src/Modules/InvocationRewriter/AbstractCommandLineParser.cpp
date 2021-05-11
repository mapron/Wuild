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

#include "AbstractCommandLineParser.h"
#include <StringUtils.h>
#include <algorithm>

namespace Wuild
{

bool AbstractCommandLineParser::IsIgnored(const std::string &arg) const
{
	return std::find(m_invocation.m_ignoredArgs.cbegin(), m_invocation.m_ignoredArgs.cend(), arg) != m_invocation.m_ignoredArgs.cend();
}

ToolInvocation AbstractCommandLineParser::GetToolInvocation() const
{
	return m_invocation;
}

void AbstractCommandLineParser::SetToolInvocation(const ToolInvocation &invocation)
{
	m_invocation = invocation;
	UpdateInfo();
	if (invocation.m_type != ToolInvocation::InvokeType::Unknown)
		m_invocation.m_type = invocation.m_type;
}

}
