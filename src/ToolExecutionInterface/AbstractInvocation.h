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
#pragma once

#include "ArgumentList.h"

#include <set>

namespace Wuild {

struct AbstractInvocationParseSettings {
    static const std::set<char> s_gnu;
    static const std::set<char> s_ms;

    bool           m_startsWithProgramPath = true; // if false, someone need to call SetProgramPath manually.
    std::set<char> m_optionStartSymbols{ s_gnu };
};

class AbstractInvocation {
public:
    struct Arg {
        std::string m_arg;              // if option, then starting symbol is excluded.
        bool        m_isOption     = false;
        char        m_optionSymbol = 0; // -, / or whatever.

        bool operator==(const Arg& rh) const noexcept;
        bool operator!=(const Arg& rh) const noexcept { return !(*this == rh); }
    };
    using ArgList = std::vector<Arg>;

public:
    explicit AbstractInvocation(const ArgumentList& parsedArguments, const AbstractInvocationParseSettings& settings = {}) noexcept;
    explicit AbstractInvocation(ArgList argList, std::string programPath) noexcept;

    void               SetProgramPath(std::string path) noexcept { m_programPath = std::move(path); }
    const std::string& GetProgramPath() const noexcept { return m_programPath; }

    const ArgList& GetArgs() const noexcept { return m_args; }
    void           SetArgs(ArgList args) noexcept { m_args = std::move(args); }
    void           SetArg(size_t index, Arg arg) noexcept { m_args[index] = std::move(arg); }

    ArgumentList ToArgumentList() const noexcept { return ToArgumentListImpl(true); }
    ArgumentList ToArgumentListWithoutProgram() const noexcept // not that pretty, ofc, but I want to find all calls without reading parameters...
    {
        return ToArgumentListImpl(false);
    }

private:
    ArgumentList ToArgumentListImpl(bool includeProgram) const noexcept;

private:
    ArgList     m_args;
    std::string m_programPath;
};

}
