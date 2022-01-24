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

#include "CommonTypes.h"

namespace Wuild {

struct ArgumentParseSettings {
    static const bool s_defaultUnescapeBehavior;
    bool              m_stripEscapeBackslashes = true; // if false, arg will continue have \\ symbol
    bool              m_stripDoubleQuotes      = true; // if false, then " will remain around arguments.
    bool              m_doUnescape             = s_defaultUnescapeBehavior;
};

struct ArgumentStringifySettings {
    bool m_checkForSpaces = true;                                             // if false, string will be outputed as is. if true, then it will be checked for space and quoted/escaped.
    bool m_escapeSpaces   = ArgumentParseSettings::s_defaultUnescapeBehavior; // if true, than use baskslash before space. If false, then quote around.
};

struct ArgumentList {
    StringVector m_args;

    std::string ToString(const ArgumentStringifySettings& settings = {}) const noexcept;
};

ArgumentList        ParseArgumentList(const StringVector& unparsedArgs, const ArgumentParseSettings& settings = {}) noexcept;
inline ArgumentList ParseArgumentList(const std::string& cmd, const ArgumentParseSettings& settings = {}) noexcept
{
    return ParseArgumentList(StringVector{ cmd }, settings);
}

}
