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

#include "MsvcCommandLineParser.h"

namespace Wuild {

bool MsvcCommandLineParser::ProcessInternal(ToolCommandline& invocation, const Options& options, bool& remotePossible) const
{
    int invokeTypeIndex = -1;
    if (!Update(invocation, &invokeTypeIndex, &remotePossible))
        return false;

    if (invocation.m_inputNameIndex == -1
        || invocation.m_outputNameIndex == -1
        || invocation.m_outputNameIndex >= (int) invocation.m_arglist.m_args.size()) {
        return false;
    }

    if (options.m_changeType != ToolCommandline::InvokeType::Unknown) {
        const bool pp = options.m_changeType == ToolCommandline::InvokeType::Preprocess;

        invocation.m_type                                             = options.m_changeType;
        invocation.m_arglist.m_args[invokeTypeIndex]                  = pp ? "/P" : "/C";
        invocation.m_arglist.m_args[invocation.m_outputNameIndex - 1] = pp ? "/Fi:" : "/Fo:";
    }
    if (options.m_removeLocalFlags) {
        StringVector newArgs;
        bool         skipNext = false;
        for (const auto& arg : invocation.m_arglist.m_args) {
            if (skipNext) {
                skipNext = false;
                continue;
            }
            if (arg == "/Fd:") {
                skipNext = true;
                continue;
            }
            if (arg == "/ZI" || arg == "/Zi") {
                newArgs.push_back("/Z7");
                continue;
            }
            if (arg == "/Gm" || arg == "/FS") {
                continue;
            }
            newArgs.push_back(arg);
        }
        invocation.m_arglist.m_args = newArgs;
    }
    if (options.m_removeDependencyFiles) {
    }
    if (options.m_removePrepocessorFlags) {
        StringVector             newArgs;
        bool                     skipNext = false;
        static const std::string s_externalInc{ "external:I" };
        for (const auto& arg : invocation.m_arglist.m_args) {
            if (skipNext) {
                skipNext = false;
                continue;
            }
            if (arg[0] == '-' || arg[0] == '/') {
                if (arg[1] == 'I' || arg[1] == 'D' || arg.substr(1) == "showIncludes") {
                    if (arg.size() == 2) {
                        skipNext = true;
                    }
                    continue;
                }
                if (arg.substr(1) == s_externalInc) {
                    skipNext = true;
                    continue;
                }
                if (arg.substr(1, s_externalInc.size()) == s_externalInc) {
                    continue;
                }
            }
            newArgs.push_back(arg);
        }
        invocation.m_arglist.m_args = newArgs;
    }
    if (options.m_removeLocalFlags || options.m_removeDependencyFiles || options.m_removePrepocessorFlags)
        if (!Update(invocation))
            return false;

    return true;
}

bool MsvcCommandLineParser::Update(ToolCommandline& invocation, int* invokeTypeIndex, bool* remotePossible) const
{
    StringVector realArgs;
    bool         ignoreNext = false;

    auto consumeArg = [&realArgs, &ignoreNext](const std::string& arg) {
        if (!arg.empty())
            realArgs.push_back(arg);
        ignoreNext = false;
    };
    auto consumeArgAndSkipNext = [&realArgs, &ignoreNext](const std::string& arg) {
        if (!arg.empty())
            realArgs.push_back(arg);
        ignoreNext = true;
    };

    invocation.m_inputNameIndex  = -1;
    invocation.m_outputNameIndex = -1;
    invocation.m_type            = ToolCommandline::InvokeType::Unknown;
    for (const auto& arg : invocation.m_arglist.m_args) {
        if (arg[0] == '/' || arg[0] == '-') {
            if (arg[1] == 'c') {
                invocation.m_type = ToolCommandline::InvokeType::Compile;
                if (invokeTypeIndex)
                    *invokeTypeIndex = realArgs.size();
            }
            if (arg[1] == 'P') {
                invocation.m_type = ToolCommandline::InvokeType::Preprocess;
                if (invokeTypeIndex)
                    *invokeTypeIndex = realArgs.size();
            }
            if (arg.size() > 3 && arg[1] == 'A' && arg[2] == 'I') {
                if (remotePossible)
                    *remotePossible = false;
            }
            if ((arg[1] == 'D' || arg[1] == 'I') && arg.size() == 2) { // /D DEFINE  /I path
                consumeArgAndSkipNext(arg);
                continue;
            }
            if (arg.substr(1) == "external:I") {
                consumeArgAndSkipNext(arg);
                continue;
            }

            if (arg[1] == 'F') {
                char fileType = arg[2];
                if (fileType == 'd'
                    || fileType == 'i'
                    || fileType == 'o') {
                    int fileIndex;
                    if (arg[3] != ':') {
                        realArgs.push_back(arg.substr(0, 3) + ":");
                        fileIndex     = realArgs.size();
                        auto filename = arg.substr(3);
                        consumeArgAndSkipNext(filename);
                    } else {
                        consumeArgAndSkipNext(arg);
                        fileIndex = realArgs.size();
                    }
                    if (fileType == 'o') {
                        invocation.m_outputNameIndex = fileIndex;
                    }
                    if (fileType == 'i') {
                        invocation.m_outputNameIndex = fileIndex; // no error here! /Fi: stands for out for preprocessor.
                    }
                    continue;
                }
            }
        } else if (!IsIgnored(invocation, arg) && !ignoreNext) {
            if (invocation.m_inputNameIndex != -1) {
                invocation.m_type = ToolCommandline::InvokeType::Unknown;
                return false;
            }
            invocation.m_inputNameIndex = realArgs.size();
        }
        consumeArg(arg);
    }
    invocation.m_arglist.m_args = realArgs;
    return true;
}

}
