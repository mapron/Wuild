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

#include "ICommandLineParser.h"

namespace Wuild
{
class AbstractCommandLineParser : public ICommandLineParser
{
protected:
	ToolInvocation m_invocation;
	bool m_remotePossible = true;

	bool IsIgnored(const std::string & arg) const;

public:
	ToolInvocation GetToolInvocation() const override;
	void SetToolInvocation(const ToolInvocation & invocation) override;

	virtual void UpdateInfo() = 0;

	bool IsRemotePossible() const override { return m_remotePossible; }
	void RemoveLocalFlags() override {}
	void RemoveDependencyFiles() override {}
	void RemovePrepocessorFlags() override {}
};

}
