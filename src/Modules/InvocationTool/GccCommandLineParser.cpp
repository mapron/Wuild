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

#include "GccCommandLineParser.h"
#include <StringUtils.h>

namespace Wuild {

bool GccCommandLineParser::ProcessInternal(ToolCommandline& invocation, const Options& options, bool& remotePossible) const
{
    int invokeTypeIndex = -1;
    if (!Update(invocation, &invokeTypeIndex))
        return false;

    if (invocation.m_inputNameIndex == -1
        || invocation.m_outputNameIndex == -1
        || invocation.m_outputNameIndex >= (int) invocation.m_arglist.m_args.size()) {
        return false;
    }
    if (options.m_changeType != ToolCommandline::InvokeType::Unknown) {
        invocation.m_type                            = options.m_changeType;
        invocation.m_arglist.m_args[invokeTypeIndex] = options.m_changeType == ToolCommandline::InvokeType::Preprocess ? "-E" : "-c";
    }
    if (options.m_removeLocalFlags) {
        StringVector newArgs;
        bool         skipNext = false;
        for (const auto& arg : invocation.m_arglist.m_args) {
            if (skipNext) {
                skipNext = false;
                continue;
            }
            if (arg == "-index-store-path") {
                skipNext = true;
                continue;
            }
            newArgs.push_back(arg);
        }
        invocation.m_arglist.m_args = newArgs;
    }
    if (options.m_removeDependencyFiles) {
        StringVector newArgs;
        bool         skipNext = false;
        for (const auto& arg : invocation.m_arglist.m_args) {
            if (skipNext) {
                skipNext = false;
                continue;
            }
            if (arg == "-MMD" || arg == "-MD")
                continue;
            if (arg == "-MF" || arg == "-MT") {
                skipNext = true;
                continue;
            }
            newArgs.push_back(arg);
        }
        invocation.m_arglist.m_args = newArgs;
    }
    if (options.m_removePrepocessorFlags) {
        StringVector newArgs;
        bool         skipNext = false;
        for (const auto& arg : invocation.m_arglist.m_args) {
            if (skipNext) {
                skipNext = false;
                continue;
            }
            if (arg.size() > 1 && arg[0] == '-') {
                if (arg[1] == 'I'
                    || arg[1] == 'D'
                    || arg[1] == 'F')
                    continue;

                if (arg == "-isysroot"
                    || arg == "-iframework"
                    || arg == "-isystem"
                    || arg == "--serialize-diagnostics"
                    || arg == "-index-store-path") {
                    skipNext = true;
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

bool GccCommandLineParser::Update(ToolCommandline& invocation, int* invokeTypeIndex) const
{
    bool skipNext = false;
    int  argIndex = -1;
    for (const auto& arg : invocation.m_arglist.m_args) {
        argIndex++;
        if (skipNext) {
            skipNext = false;
            continue;
        }
        if (arg.size() > 1 && arg[0] == '-') {
            if (arg[1] == 'c') {
                if (invokeTypeIndex)
                    *invokeTypeIndex = argIndex;
                invocation.m_type = ToolCommandline::InvokeType::Compile;
            }
            if (arg[1] == 'E') {
                if (invokeTypeIndex)
                    *invokeTypeIndex = argIndex;
                invocation.m_type = ToolCommandline::InvokeType::Preprocess;
            }
            if (arg[1] == 'o') {
                invocation.m_outputNameIndex = argIndex + 1;
                skipNext                     = true;
            }
            if (arg[1] == 'x') {
                skipNext = true;
            }
            if (arg == "-MF" || arg == "-MT" || arg == "-isysroot" || arg == "-target" || arg == "-isystem" || arg == "-iframework" || arg == "--serialize-diagnostics" || arg == "-index-store-path" || arg == "-arch")
                skipNext = true;

            continue;
        }
        if (!IsIgnored(invocation, arg)) {
            if (invocation.m_inputNameIndex != -1) {
                return false;
            }
            invocation.m_inputNameIndex = argIndex;
        }
    }
    return true;
}

}
