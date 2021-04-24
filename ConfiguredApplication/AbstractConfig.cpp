/*
 * Copyright (C) 2017 Smirnov Vladimir mapron1@gmail.com
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

#include "AbstractConfig.h"

#include <FileUtils.h>
#include <StringUtils.h>
#include <Application.h>
#include <Syslogger.h>

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <fstream>
#include <sstream>
#include <regex>

namespace
{
	const char g_listSeparator = ',';
	const char g_equal = '=';
	const char g_comment = ';';
	const char g_iniGroupStart = '[';
	const char g_groupCommandLineSeparator = '-';
	const std::string g_assign = ":=";
}

namespace Wuild
{

void AbstractConfig::ReadCommandLine(const StringVector & args)
{
	for (const auto & arg : args)
	{
		const auto groupPos = arg.find(g_groupCommandLineSeparator);
		if (groupPos == std::string::npos)
			SetArg("", arg);
		else
			SetArg(arg.substr(0, groupPos), arg.substr(groupPos + 1));
	}
}

bool AbstractConfig::ReadIniFile(const std::string &filename)
{
	if (filename.empty())
		return false;

	std::string currentGroup;

	std::map<std::string, std::string> variables;

	std::ifstream fin;
	fin.open(filename);
	if (!fin)
		return false;
	std::string line;
	const std::regex varregex { R"regex(\$\w+)regex" };
	while(fin){
		 std::getline(fin, line);
		 line = StringUtils::Trim(line);
		 line = line.substr(0, line.find(g_comment)); // remove comments;
		 if (!line.empty())
		 {
			if (line[0] == g_iniGroupStart)
			{
				currentGroup = line.substr(1, line.size()-2); // hope we have closing ].
				continue;
			}

			// Replace all $VarName in line. First, trying our variable map; second, try environment variable.
			size_t offset = 0;
			std::smatch match;
			while (std::regex_search(line.cbegin() + offset, line.cend(), match, varregex))
			{
				auto wholeMatchPosition = match.position(0);
				const std::string varName = match[0].str().substr(1);
				std::string replacement = "???";
				const auto varIt = variables.find(varName);
				if (varIt != variables.cend())
				{
					replacement = varIt->second;
				}
				else
				{
					auto envValue = getenv(varName.c_str());
					if (envValue)
						replacement = envValue;
					else
						Syslogger(Syslogger::Err) << "Invalid variable '" << varName << "' found in config.";
				}

				const auto start = line.begin() + offset + wholeMatchPosition;
				line.replace(start, start + match[0].length(), replacement);

				offset += wholeMatchPosition + replacement.length();
			}

			size_t assignPos = line.find(g_assign);
			if (assignPos != std::string::npos)
			{
				const auto varname = StringUtils::Trim(line.substr(0, assignPos));
				const auto value = StringUtils::Trim(line.substr(assignPos + g_assign.size()));
				variables[varname] = value;
				continue;
			}
			SetArg(currentGroup, line);
		 }
	}
	fin.close();
	return true;
}

bool AbstractConfig::empty() const
{
	return m_data.empty();
}

bool AbstractConfig::Exists(const std::string & group, const std::string &key) const
{
	return Find(group, key) != nullptr;
}

int AbstractConfig::GetInt(const std::string & group, const std::string &key, int defValue) const
{
	const auto * val = Find(group, key);
	if (!val)
		return defValue;

	return std::atoi(val->c_str());
}

double AbstractConfig::GetDouble(const std::string & group, const std::string & key, double defValue) const
{
	const auto * val = Find(group, key);
	if (!val)
		return defValue;

	return std::stod(*val);
}

std::string AbstractConfig::GetString(const std::string & group, const std::string &key, const std::string &defValue) const
{
	const auto * val = Find(group, key);
	if (!val)
		return defValue;

	return *val;
}

StringVector AbstractConfig::GetStringList(const std::string & group, const std::string &key, const StringVector &defValue) const
{
	const auto * val = Find(group, key);
	if (!val)
		return defValue;

	StringVector res;
	StringUtils::SplitString(*val, res, g_listSeparator, true, true);
	return res;
}

bool AbstractConfig::GetBool(const std::string & group, const std::string &key, bool defValue) const
{
	const std::string val = GetString(group, key);
	if (val == "true" || val == "TRUE" || val == "ON" || val == "on")
		return true;

	return GetInt(group, key, defValue) > 0;
}

std::string AbstractConfig::DumpAllValues() const
{
	std::ostringstream os;
	for (const auto& group : m_data)
	{
		if (!group.first.empty())
		{
			os << "\n\n[" << group.first << "]";
		}
		for (const auto & keyValue : group.second)
		{
			os << "\n" << keyValue.first << " = " << keyValue.second;
		}
	}
	return os.str();
}

void AbstractConfig::SetArg(const std::string & group, const std::string &arg)
{
	const auto equalPos = arg.find(g_equal);
	if (equalPos == std::string::npos)
		return;

	const auto key = StringUtils::Trim(arg.substr(0, equalPos));
	const auto value = StringUtils::Trim(arg.substr(equalPos + 1));
	m_data[group][key] = value;
}

const std::string *AbstractConfig::Find(const std::string &group, const std::string &key) const
{
	auto groupIt = m_data.find(group);
	if (groupIt == m_data.end())
		return group.empty() ? nullptr : Find("", key);

	auto valueIt =  groupIt->second.find(key);
	if (valueIt == groupIt->second.end())
		return group.empty() ? nullptr : Find("", key);

	return &(valueIt->second);
}


}
