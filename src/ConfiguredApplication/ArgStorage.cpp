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

#include "ArgStorage.h"

#include "StringUtils.h"

namespace
{
const std::string g_commandLinePrefix = "--wuild-";
}

namespace Wuild
{

ArgStorage::ArgStorage(int & argcRef, char **& argvRef)
	: m_argv(1)
{
	m_argv[0] = argvRef[0];
	StringVector unprefixed;
	const auto inputArgs = StringUtils::StringVectorFromArgv(argcRef, argvRef);
	for (const auto & arg : inputArgs)
	{
		if (arg.find(g_commandLinePrefix) == 0)
			m_configValues.push_back(arg.substr(g_commandLinePrefix.size()));
		else
			m_args.push_back(arg);
	}
	m_argv.resize(m_args.size() + 2);
	for (size_t i = 0; i < m_args.size(); ++i)
		m_argv[i+1] = const_cast<char*>(m_args[i].data()); // non-const .data() is C++17+ only.
	m_argv[m_argv.size() - 1] = nullptr;

	argcRef = m_argv.size() - 1;

	argvRef = m_argv.data();
}

const StringVector & ArgStorage::GetConfigValues() const
{
	return m_configValues;
}

const StringVector & ArgStorage::GetArgs() const
{
	return m_args;
}

}
