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
#include "IConfig.h"

namespace Wuild
{
class CompilerConfig : public IConfig
{
public:
	enum class ToolchainType { GCC, MSVC };
	struct CompilerUnit
	{
		std::string m_id;
		std::string m_appendOption;
		ToolchainType m_type = ToolchainType::GCC;
		std::vector<std::string> m_names;
	};
	std::vector<CompilerUnit> m_modules;
	StringVector m_toolIds;
	std::string GetFirstToolId() const;
	std::string GetFirstToolName() const;
	bool Validate(std::ostream * errStream = nullptr) const override;
};
}
