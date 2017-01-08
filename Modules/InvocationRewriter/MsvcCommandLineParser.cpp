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

#include "MsvcCommandLineParser.h"

namespace Wuild
{

void MsvcCommandLineParser::UpdateInfo()
{
	StringVector realArgs;
	bool skipNext = false;
	bool ignoreNext = false;

	m_invocation.m_inputNameIndex  = -1;
	m_invocation.m_outputNameIndex = -1;
	m_invocation.m_type = ToolInvocation::InvokeType::Unknown;
	for (const auto & arg : m_invocation.m_args)
	{
	   // argIndex++;
		if (skipNext)
		{
			skipNext = false;
			continue;
		}
		if (arg[0] == '/' || arg[0] == '-')
		{
			if (arg[1] == 'c' )
			{
				m_invocation.m_type = ToolInvocation::InvokeType::Compile;
				m_invokeTypeIndex = realArgs.size();
			}
			if (arg[1] == 'P' )
			{
				m_invocation.m_type = ToolInvocation::InvokeType::Preprocess;
				m_invokeTypeIndex = realArgs.size();
			}

			if (arg[1] == 'F' )
			{
				char fileType = arg[2];
				if (fileType == 'd' || fileType == 'i' || fileType == 'o')
				{
				   int fileIndex;
				   if (arg[3] != ':')
				   {
					   realArgs.push_back(arg.substr(0,3) + ":");
					   fileIndex = realArgs.size();
					   auto filename = arg.substr(3);
					   if (!filename.empty())
						  realArgs.push_back(filename);
					   ignoreNext = true;
				   }
				   else
				   {
					   realArgs.push_back(arg);
					   ignoreNext = true;
					   fileIndex = realArgs.size();
				   }
				   if (fileType == 'o')
				   {
						m_invocation.m_outputNameIndex = fileIndex;
				   }
				   if (fileType == 'i')
				   {
						m_invocation.m_outputNameIndex = fileIndex;
				   }
				   continue;
				}
			}
		}
		else if (!IsIgnored(arg) && !ignoreNext)
		{
			if (m_invocation.m_inputNameIndex != -1)
			{
				m_invocation.m_type = ToolInvocation::InvokeType::Unknown;
				return;
			}
			m_invocation.m_inputNameIndex = realArgs.size();
		}
		realArgs.push_back(arg);
		ignoreNext = false;
	}
	m_invocation.m_args = realArgs;
	if (m_invocation.m_inputNameIndex == -1 || m_invocation.m_outputNameIndex == -1 || m_invocation.m_outputNameIndex >= (int)m_invocation.m_args.size())
	{
		m_invocation.m_inputNameIndex = -1;
		m_invocation.m_outputNameIndex = -1;
		m_invocation.m_type = ToolInvocation::InvokeType::Unknown;
	}
}

void MsvcCommandLineParser::SetInvokeType(ToolInvocation::InvokeType type)
{
	if (m_invocation.m_type == ToolInvocation::InvokeType::Unknown)
		return;
	bool pp = type == ToolInvocation::InvokeType::Preprocess;
	m_invocation.m_type = type;
	m_invocation.m_args[m_invokeTypeIndex] = pp ? "/P" : "/C";
	m_invocation.m_args[m_invocation.m_outputNameIndex - 1] = pp ? "/Fi:" : "/Fo:";
}


void MsvcCommandLineParser::RemovePDB()
{
	StringVector newArgs;
	bool skipNext = false;
	for (const auto & arg : m_invocation.m_args)
	{
		if (skipNext)
		{
			skipNext = false;
			continue;
		}
		if (arg == "/Fd:")
		{
			skipNext = true;
			continue;
		}
		if (arg == "/ZI" || arg == "/Zi")
		{
			newArgs.push_back("/Z7");
			continue;
		}
		if (arg == "/Gm" || arg == "/FS")
		{
			continue;
		}
		newArgs.push_back(arg);
	}
	m_invocation.m_args = newArgs;
	UpdateInfo();
}

void MsvcCommandLineParser::RemovePrepocessorFlags()
{
	StringVector newArgs;
	for (const auto & arg : m_invocation.m_args)
	{
		if (arg[0] == '-' || arg[0] == '/')
		{
			if (arg[1] == 'I' || arg[1] == 'D' || arg.substr(1) == "showIncludes")
			   continue;
		}
		newArgs.push_back(arg);
	}
	m_invocation.m_args = newArgs;
	UpdateInfo();
}

}
