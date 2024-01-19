/*
 * Copyright (C) 2021 Smirnov Vladimir mapron1@gmail.com
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

#include "StatusWriter.h"

#include "MernelPlatform/PropertyTree.hpp"

#include <iostream>

using Mernel::PropertyTree;
using Mernel::PropertyTreeScalar;

StandardTextWriter::StandardTextWriter(std::ostream& stream)
    : m_ostream(stream)
{
}

StandardTextWriter::~StandardTextWriter()
{
    m_ostream << std::endl;
}

void StandardTextWriter::FormatMessage(const std::string& msg)
{
    m_ostream << msg;
}

void StandardTextWriter::FormatConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads)
{
    m_ostream << host << ":" << port << " load:" << runningTasks << "/" << totalThreads << std::endl;
}

void StandardTextWriter::FormatSummary(int running, int thread, int queued)
{
    m_ostream << std::endl
              << "Total load:" << running << "/" << thread << ", queue: " << queued << std::endl;
}

void StandardTextWriter::FormatSessions(const std::string& started, int usedThreads)
{
    m_ostream << "Started " << started << ", used: " << usedThreads << std::endl;
}
void StandardTextWriter::FormatOverallTools(const std::set<std::string>& toolIds)
{
    m_ostream << "\nAvailable tools: ";
    for (const auto& t : toolIds)
        m_ostream << t << ", ";
    m_ostream << "\n";
}

void StandardTextWriter::FormatToolsByToolsServer(const AbstractWriter::ToolsMap& toolsByHost)
{
    m_ostream << "\nAvailable tools by toolserver: \n";
    for (const auto& toolserver : toolsByHost) {
        m_ostream << "\t" << toolserver.first << ":\t";
        for (const auto& toolId : toolserver.second)
            m_ostream << toolId << ", ";
        m_ostream << "\n";
    }
}

struct JsonWriter::Impl {
    PropertyTree jsonResult;
};

JsonWriter::JsonWriter(std::ostream& stream)
    : m_impl(new Impl)
    , m_ostream(stream){};

JsonWriter::~JsonWriter()
{
    PropertyTree::printReadableJson(m_ostream, m_impl->jsonResult);
    m_ostream << std::endl;
}

void JsonWriter::FormatMessage(const std::string&)
{
    // stub
}
void JsonWriter::FormatConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads)
{
    m_impl->jsonResult["connected_tool_servers"][host]["port"]          = PropertyTreeScalar(port);
    m_impl->jsonResult["connected_tool_servers"][host]["running_tasks"] = PropertyTreeScalar(runningTasks);
    m_impl->jsonResult["connected_tool_servers"][host]["total_threads"] = PropertyTreeScalar(totalThreads);
}

void JsonWriter::FormatSummary(int running, int thread, int queued)
{
    m_impl->jsonResult["total_load"]["running"] = PropertyTreeScalar(running);
    m_impl->jsonResult["total_load"]["thread"]  = PropertyTreeScalar(thread);
    m_impl->jsonResult["total_load"]["queue"]   = PropertyTreeScalar(queued);
}

void JsonWriter::FormatSessions(const std::string& started, int usedThreads)
{
    auto& sessions = m_impl->jsonResult["Running sessions"];
    sessions.convertToList();
    sessions.getList().push_back(PropertyTree(Mernel::PropertyTreeMap{ { "started", PropertyTreeScalar(started) }, { "usedThreads", PropertyTreeScalar(usedThreads) } }));
}
void JsonWriter::FormatOverallTools(const std::set<std::string>& toolIds)
{
    auto& toolIdsList = m_impl->jsonResult["available_tools"];
    toolIdsList.convertToList();
    for (const auto& str : toolIds)
        toolIdsList.getList().push_back(PropertyTreeScalar(str));
}

void JsonWriter::FormatToolsByToolsServer(const AbstractWriter::ToolsMap& toolsByHost)
{
    auto& toolsByHostJson = m_impl->jsonResult["available_tools_by_host"];
    for (const auto& toolserver : toolsByHost) {
        auto& toolList = toolsByHostJson[toolserver.first];
        toolList.convertToList();
        for (const auto& str : toolserver.second)
            toolList.getList().push_back(PropertyTreeScalar(str));
    }
}

std::unique_ptr<AbstractWriter> AbstractWriter::createWriter(OutType outType, std::ostream& stream)
{
    if (outType == OutType::JSON)
        return std::make_unique<JsonWriter>(stream);
    return std::make_unique<StandardTextWriter>(stream);
}
