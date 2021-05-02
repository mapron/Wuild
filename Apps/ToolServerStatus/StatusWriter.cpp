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

#include "json.hpp"

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

void StandardTextWriter::FormatToolsVersions(const std::string & host, const AbstractWriter::ToolsMap& toolsVersions)
{
	m_ostream << "\t" <<host << ":\n";
	for (const auto & toolId : toolsVersions)
		m_ostream << "\t\t"<< toolId.first << "=\"" << toolId.second << "\"\n";
	m_ostream << "\n";

	std::flush(m_ostream);
}


struct JsonWriter::Impl
{
	nlohmann::ordered_json jsonResult;
};

JsonWriter::JsonWriter(std::ostream & stream)
	: m_impl(new Impl)
	, m_ostream(stream) {
};

JsonWriter::~JsonWriter() {
	m_ostream << m_impl->jsonResult.dump(4) << std::endl;
}

void JsonWriter::FormatMessage(const std::string&)
{
	// stub
}

void JsonWriter::FormatToolsVersions(const std::string & host, const AbstractWriter::ToolsMap& toolsByHost)
{
	auto & toolsByHostJson = m_impl->jsonResult["tools_versions"];
	toolsByHostJson[host] = toolsByHost;
}

std::unique_ptr<AbstractWriter> AbstractWriter::createWriter(OutType outType, std::ostream & stream)
{
	if (outType == OutType::JSON)
		return std::make_unique<JsonWriter> (stream);
	return std::make_unique<StandardTextWriter> (stream);
}
