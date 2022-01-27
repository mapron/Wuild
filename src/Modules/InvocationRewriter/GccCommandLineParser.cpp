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

void GccCommandLineParser::UpdateInfo()
{
    bool skipNext                  = false;
    int  argIndex                  = -1;
    m_invocation.m_inputNameIndex  = -1;
    m_invocation.m_outputNameIndex = -1;
    m_invocation.m_type            = ToolCommandline::InvokeType::Unknown;
    for (const auto& arg : m_invocation.m_arglist.m_args) {
        argIndex++;
        if (skipNext) {
            skipNext = false;
            continue;
        }
        if (arg.size() > 1 && arg[0] == '-') {
            if (arg[1] == 'c') {
                m_invokeTypeIndex   = argIndex;
                m_invocation.m_type = ToolCommandline::InvokeType::Compile;
            }
            if (arg[1] == 'E') {
                m_invokeTypeIndex   = argIndex;
                m_invocation.m_type = ToolCommandline::InvokeType::Preprocess;
            }
            if (arg[1] == 'o') {
                m_invocation.m_outputNameIndex = argIndex + 1;
                skipNext                       = true;
            }
            if (arg[1] == 'x') {
                skipNext = true;
            }
            if (arg == "-MF" || arg == "-MT" || arg == "-isysroot" || arg == "-target" || arg == "-isystem" || arg == "-iframework" || arg == "--serialize-diagnostics" || arg == "-index-store-path" || arg == "-arch")
                skipNext = true;

            continue;
        }
        if (!IsIgnored(arg)) {
            if (m_invocation.m_inputNameIndex != -1) {
                m_invocation.m_type = ToolCommandline::InvokeType::Unknown;
                return;
            }
            m_invocation.m_inputNameIndex = argIndex;
        }
    }
    if (m_invocation.m_inputNameIndex == -1 || m_invocation.m_outputNameIndex == -1 || m_invocation.m_outputNameIndex >= (int) m_invocation.m_arglist.m_args.size()) {
        m_invocation.m_inputNameIndex  = -1;
        m_invocation.m_outputNameIndex = -1;
        m_invocation.m_type            = ToolCommandline::InvokeType::Unknown;
    }
}

void GccCommandLineParser::SetInvokeType(ToolCommandline::InvokeType type)
{
    if (m_invocation.m_type == ToolCommandline::InvokeType::Unknown)
        return;

    m_invocation.m_type                              = type;
    m_invocation.m_arglist.m_args[m_invokeTypeIndex] = type == ToolCommandline::InvokeType::Preprocess ? "-E" : "-c";
}

void GccCommandLineParser::RemoveLocalFlags()
{
    StringVector newArgs;
    bool         skipNext = false;
    for (const auto& arg : m_invocation.m_arglist.m_args) {
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
    m_invocation.m_arglist.m_args = newArgs;
    UpdateInfo();
}

void GccCommandLineParser::RemoveDependencyFiles()
{
    StringVector newArgs;
    bool         skipNext = false;
    for (const auto& arg : m_invocation.m_arglist.m_args) {
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
    m_invocation.m_arglist.m_args = newArgs;
    UpdateInfo();
}

void GccCommandLineParser::RemovePrepocessorFlags()
{
    StringVector newArgs;
    bool         skipNext = false;
    for (const auto& arg : m_invocation.m_arglist.m_args) {
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
    m_invocation.m_arglist.m_args = newArgs;
    UpdateInfo();
}

}
