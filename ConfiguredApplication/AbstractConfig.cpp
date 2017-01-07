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

#include "FileUtils.h"
#include "StringUtils.h"
#include "Application.h"

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <fstream>
#include <sstream>

namespace
{
	const char g_listSeparator = ',';
	const char g_equal = '=';
	const char g_comment = ';';
	const char g_iniGroupStart = '[';
	const char g_groupCommandLineSeparator = '-';
}

namespace Wuild
{

StringVector AbstractConfig::ReadCommandLine(const StringVector &args, const std::string &prefix)
{
	StringVector unprefixed;
	for (const auto & arg : args)
	{
		if (arg.find(prefix) == 0)
		{
			const auto argNoPrefix = arg.substr(prefix.size());
			const auto groupPos = argNoPrefix.find(g_groupCommandLineSeparator);
			if (groupPos == std::string::npos)
				SetArg("", argNoPrefix);
			else
				SetArg(argNoPrefix.substr(0, groupPos), argNoPrefix.substr(groupPos + 1));
		}
		else
		{
			unprefixed.push_back(arg);
		}
	}
	return unprefixed;
}

bool AbstractConfig::ReadIniFile(const std::string &filename)
{
	if (filename.empty())
		return false;

	std::string currentGroup;

	std::ifstream fin ;
	fin.open(filename);
	if (!fin)
		return false;
	std::string line;
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
		groupIt = m_data.find(""); // try to find default group

	if (groupIt == m_data.end())
		return nullptr;

	auto valueIt =  groupIt->second.find(key);
	if (valueIt == groupIt->second.end())
		return nullptr;

	return &(valueIt->second);
}


}
