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

#include "CompilerInvocation.h"

#include <StringUtils.h>

namespace Wuild
{
CompilerInvocation::CompilerInvocation(const StringVector &args, InvokeType type)
    : m_args(args)
    , m_type(type)
{

}

CompilerInvocation::CompilerInvocation(const std::string &args, CompilerInvocation::InvokeType type)
    : m_type(type)
{
    SetArgsString(args);
}
void CompilerInvocation::SetArgsString(const std::string &args)
{
    m_args.resize(1);
    m_args[0] = args;
}

std::string CompilerInvocation::GetArgsString(bool prependExecutable) const
{
    std::string ret;
    if (prependExecutable)
        ret += m_id.m_compilerExecutable + " ";
    return ret + StringUtils::JoinString( m_args, ' ');
}

bool CompilerInvocation::SetInput(const std::string &filename)
{
    if (m_inputNameIndex < 0)
        return false;

    m_args[m_inputNameIndex] = filename;
    return true;
}

std::string CompilerInvocation::GetInput() const
{
    if (m_inputNameIndex < 0)
        return std::string();

    return m_args[m_inputNameIndex];
}

bool CompilerInvocation::SetOutput(const std::string &filename)
{
    if (m_outputNameIndex < 0)
        return false;

    m_args[m_outputNameIndex] = filename;
    return true;
}

std::string CompilerInvocation::GetOutput() const
{
    if (m_outputNameIndex < 0)
        return std::string();

    return m_args[m_outputNameIndex];
}



CompilerInvocation &CompilerInvocation::SetId(const std::string &compilerId)
{
    m_id.m_compilerId = compilerId;
    return *this;
}

CompilerInvocation &CompilerInvocation::SetExecutable(const std::string &compilerExecutable)
{
    m_id.m_compilerExecutable = compilerExecutable;
    return *this;
}

}
