/*
 * Copyright (C) 2017-2021 Smirnov Vladimir mapron1@gmail.com
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

namespace Wuild {
ToolInvocation::ToolInvocation(StringVector args, InvokeType type)
    : m_type(type)
{
    m_arglist = ParseArgumentList(args);
}

ToolInvocation::ToolInvocation(const std::string& args, ToolInvocation::InvokeType type)
    : m_type(type)
{
    SetArgsString(args);
}

void ToolInvocation::FetchExecutableFromArgs()
{
    assert(m_id.m_toolExecutable.empty());
    assert(!m_arglist.m_args.empty());

    SetExecutable(m_arglist.m_args[0]);
    m_arglist.m_args.erase(m_arglist.m_args.begin());
}

void ToolInvocation::SetArgsString(const std::string& args)
{
    m_arglist = ParseArgumentList(args);
}

std::string ToolInvocation::GetArgsString() const
{
    return m_arglist.ToString();
}

bool ToolInvocation::SetInput(const std::string& filename)
{
    if (m_inputNameIndex < 0)
        return false;

    m_arglist.m_args[m_inputNameIndex] = filename;
    return true;
}

std::string ToolInvocation::GetInput() const
{
    if (m_inputNameIndex < 0)
        return std::string();

    return m_arglist.m_args[m_inputNameIndex];
}

bool ToolInvocation::SetOutput(const std::string& filename)
{
    if (m_outputNameIndex < 0)
        return false;

    m_arglist.m_args[m_outputNameIndex] = filename;
    return true;
}

std::string ToolInvocation::GetOutput() const
{
    if (m_outputNameIndex < 0)
        return std::string();

    return m_arglist.m_args[m_outputNameIndex];
}

ToolInvocation& ToolInvocation::SetId(const std::string& toolId)
{
    m_id.m_toolId = toolId;
    return *this;
}

ToolInvocation& ToolInvocation::SetExecutable(const std::string& toolExecutable)
{
    m_id.m_toolExecutable = toolExecutable;
    return *this;
}

}
