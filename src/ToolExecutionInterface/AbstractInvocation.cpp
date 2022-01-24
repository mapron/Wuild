/*
 * Copyright (C) 2022 Smirnov Vladimir mapron1@gmail.com
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
#include "AbstractInvocation.h"

#include <cassert>

namespace Wuild {

const std::set<char> AbstractInvocationParseSettings::s_gnu{ '-' };
const std::set<char> AbstractInvocationParseSettings::s_ms{ '-', '/' };

AbstractInvocation::AbstractInvocation(const ArgumentList& parsedArguments, const AbstractInvocationParseSettings& settings) noexcept
{
    bool skipFirst = settings.m_startsWithProgramPath;
    if (settings.m_startsWithProgramPath && !parsedArguments.m_args.empty())
        m_programPath = parsedArguments.m_args[0];
    for (const std::string& arg : parsedArguments.m_args) {
        if (skipFirst) {
            skipFirst = false;
            continue;
        }
        assert(!arg.empty());
        Arg a;
        a.m_arg          = arg;
        const char first = arg[0];
        if (settings.m_optionStartSymbols.count(first)) {
            a.m_isOption     = true;
            a.m_optionSymbol = first;
            a.m_arg.erase(0, 1);
        }
        m_args.push_back(std::move(a));
    }
}

AbstractInvocation::AbstractInvocation(ArgList argList, std::string programPath) noexcept
    : m_args(std::move(argList))
    , m_programPath(std::move(programPath))
{
}

ArgumentList AbstractInvocation::ToArgumentListImpl(bool includeProgram) const noexcept
{
    ArgumentList result;
    if (includeProgram && !m_programPath.empty()) // for the consistency of roundabout for empty args.
        result.m_args.push_back(m_programPath);

    for (const Arg& arg : m_args) {
        if (arg.m_isOption)
            result.m_args.push_back(arg.m_optionSymbol + arg.m_arg);
        else
            result.m_args.push_back(arg.m_arg);
    }

    return result;
}

bool AbstractInvocation::Arg::operator==(const Arg& rh) const noexcept
{
    return true
           && m_isOption == rh.m_isOption
           && m_optionSymbol == rh.m_optionSymbol
           && m_arg == rh.m_arg;
}

}
