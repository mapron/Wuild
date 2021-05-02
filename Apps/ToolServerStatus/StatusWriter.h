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

#pragma once

#include <memory>
#include <string>
#include <map>
#include <ios>

class AbstractWriter
{
public:
	enum class OutType{
		JSON,
		STD_TEXT,
	};

	using ToolsMap = std::map<std::string, std::string>;

	virtual ~AbstractWriter() = default;
	virtual void FormatMessage(const std::string& msg) = 0;
	virtual void FormatToolsVersions(const std::string & host, const ToolsMap& toolsByHost) = 0;

	static std::unique_ptr<AbstractWriter> createWriter(AbstractWriter::OutType outType, std::ostream & stream);
};

class StandardTextWriter: public AbstractWriter
{
public:
	StandardTextWriter(std::ostream & stream);
	~StandardTextWriter();
	void FormatMessage(const std::string& msg) override;
	void FormatToolsVersions(const std::string & host, const ToolsMap& toolsByHost) override;

private:
	std::ostream & m_ostream;
};

class JsonWriter: public AbstractWriter
{
public:
	JsonWriter(std::ostream & ostream);
	~JsonWriter();
	void FormatMessage(const std::string& msg) override;
	void FormatToolsVersions(const std::string & host, const ToolsMap& toolsByHost) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	std::ostream & m_ostream;
};
