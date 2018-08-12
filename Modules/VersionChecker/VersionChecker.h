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

#pragma once

#include "IVersionChecker.h"
#include "IInvocationRewriter.h"

#include <ILocalExecutor.h>

namespace Wuild
{
class VersionChecker : public IVersionChecker
{
	VersionChecker(ILocalExecutor::Ptr localExecutor, IInvocationRewriter::Ptr rewriter);
public:
	static Ptr Create(ILocalExecutor::Ptr localExecutor, IInvocationRewriter::Ptr rewriter)
	{ return Ptr(new VersionChecker(std::move(localExecutor), std::move(rewriter))); }

	ToolType GuessToolType(const ToolInvocation::Id & toolId) const override;

	VersionMap DetermineToolVersions(const std::vector<std::string> & toolIds) const override;

private:
	Version GetToolVersion(const ToolInvocation::Id & toolId, const std::string & envCommand, ToolType type) const;

private:
	ILocalExecutor::Ptr      m_localExecutor;
	IInvocationRewriter::Ptr m_rewriter;
};

}
