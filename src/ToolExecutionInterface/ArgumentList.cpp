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
#include "ArgumentList.h"
#include "StringUtils.h"

namespace Wuild {

namespace {

void InplaceReplace(std::string& input, const char find, const std::string& replace)
{
    size_t startPos = 0;
    while ((startPos = input.find(find, startPos)) != std::string::npos) {
        input.replace(startPos, 1, replace);
        startPos += replace.length();
    }
}

} // anonymous namespace

// Windows usually use backslashes as path separator, so ignore them by default.
const bool ArgumentParseSettings::s_defaultUnescapeBehavior =
#ifdef _WIN32
    false
#else
    true
#endif
    ;

ArgumentList ParseArgumentList(const StringVector& unparsedArgs, const ArgumentParseSettings& settings) noexcept
{
    ArgumentList args;
    std::string  current;
    bool         startedQuote = false;
    auto         consume      = [&current, &args](bool force) {
        if (!current.empty() && *current.rbegin() == ' ')
            current = current.size() == 1 ? "" : current.substr(0, current.size() - 1);

        if (force || !current.empty()) {
            args.m_args.push_back(std::move(current));
        }
        current.clear();
    };
    bool escapedNext = false;
    for (const auto& arg : unparsedArgs) {
        for (char c : arg) {
            if (escapedNext) {
                escapedNext = false;
                current += c;
            } else if (startedQuote) {
                if (c == '"') {
                    startedQuote = false;
                    if (!settings.m_stripDoubleQuotes)
                        current += c;
                    consume(true);
                } else {
                    current += c;
                }
            } else {
                if (c == '"') {
                    startedQuote = true;
                    if (!settings.m_stripDoubleQuotes)
                        current += c;
                } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                    consume(false);
                } else if (settings.m_doUnescape && c == '\\') {
                    escapedNext = true;
                    if (!settings.m_stripEscapeBackslashes)
                        current += c;
                } else {
                    current += c;
                }
            }
        }
        if (!startedQuote)
            consume(false);
        else
            current += ' ';
    }

    consume(false);
    return args;
}

std::string ArgumentList::ToString(const ArgumentStringifySettings& settings) const noexcept
{
    if (!settings.m_checkForSpaces)
        return StringUtils::JoinString(m_args, ' ');
    StringVector             escapedArgs;
    const bool               escapeSpaces = settings.m_escapeSpaces;
    static const std::string s_spaceReplacement{ "\\ " };
    for (std::string arg : m_args) {
        if (arg.find(' ') != std::string::npos) {
            if (!escapeSpaces)
                arg = '"' + arg + '"';
            else
                InplaceReplace(arg, ' ', s_spaceReplacement);
        }
        escapedArgs.push_back(std::move(arg));
    }
    return StringUtils::JoinString(escapedArgs, ' ');
}

void ArgumentList::TryFixDoubleQuotes()
{
    bool hasCorrectFormat = true;
    for (const std::string& arg : m_args) {
        if (arg == "\"") {
            hasCorrectFormat = false;
            break;
        } else if (arg.size() >= 2) {
            char b = *arg.begin();
            char e = *arg.rbegin();
            if ((b == '"' && e != '"') || (e == '"' && b != '"')) {
                hasCorrectFormat = false;
                break;
            }
        }
    }
    if (!hasCorrectFormat) {
        *this = ParseArgumentList(m_args);
    }
}

}
