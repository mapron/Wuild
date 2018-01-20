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

#include "StringUtils.h"

#include <algorithm>
#include <sstream>
#include <functional>

namespace Wuild { namespace StringUtils {

std::string JoinString(const StringVector &list, char glue)
{
	std::string ret;
	for (size_t i =0 ; i< list.size(); ++i)
	{
		if (i > 0)
			ret += glue;
		ret += list[i];
	}
	return ret;
}

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

std::string Trim(const std::string & s) {
	auto result = s;
	ltrim(result);
	rtrim(result);
	return result;
}

void SplitString(const std::string & str, StringVector & outList, char delim, bool trimEach, bool skipEmpty)
{
	std::istringstream ss(str);
	std::string item;
	while (std::getline(ss, item, delim))
	{
		if (trimEach)
			Trim(item);
		if (skipEmpty && item.empty())
			continue;

		outList.push_back(item);
	}
}

StringVector StringVectorFromArgv(int argc, char **argv, bool skipFirstArg)
{
	StringVector ret;
	for (int i = int(skipFirstArg); i < argc; ++i)
	{
		std::string arg(argv[i]);
		std::string escapedArg = arg;
		if (arg.find(' ') != std::string::npos && arg[0] != '"')
			escapedArg = '"' + arg + '"';

		ret.emplace_back(escapedArg);
	}
	return ret;
}

} }
