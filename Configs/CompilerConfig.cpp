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

#include "CompilerConfig.h"

#include <iostream>

namespace Wuild
{

std::string CompilerConfig::GetFirstToolId() const
{
	return m_modules.empty() ? "" : m_modules[0].m_id;
}

std::string CompilerConfig::GetFirstToolName() const
{
	return m_modules.empty() || m_modules[0].m_names.empty() ? "" : m_modules[0].m_names[0];
}

bool CompilerConfig::Validate(std::ostream *errStream) const
{
	if (m_modules.empty())
	{
		if (errStream)
			*errStream << "Compiler modules are empty.";
		return false;
	}
	for (const auto & unit : m_modules)
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

}
