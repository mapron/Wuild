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

#include "Syslogger_private.h"
#include "Syslogger.h"

#include <iostream>
#include <iomanip>

namespace Wuild
{

std::unique_ptr<ILoggerBackend> g_loggerBackend(new LoggerBackendConsole(Syslogger::Notice, false, false, false, true));

void Syslogger::SetLoggerBackend(std::unique_ptr<ILoggerBackend> && backend)
{
	g_loggerBackend = std::move(backend);
}

bool Syslogger::IsLogLevelEnabled(int logLevel)
{
	return g_loggerBackend->LogEnabled(logLevel);
}

Syslogger::Syslogger(int logLevel)
	: m_logLevel(logLevel)
{
	if (g_loggerBackend->LogEnabled(logLevel))
		m_stream.reset(new std::ostringstream());
}

Syslogger::Syslogger(const std::string &context, int logLevel)
{
	if (g_loggerBackend->LogEnabled(logLevel))
	{
		m_stream.reset(new std::ostringstream());
		if (!context.empty())
		   *m_stream << "{" << context << "} ";
	}
}

Syslogger::~Syslogger()
{
	if (m_stream)
		g_loggerBackend->FlushMessage(m_stream->str(), m_logLevel);
}

Syslogger::operator bool () const
{
	return !!m_stream;
}

Syslogger& Syslogger::operator << (const char *str)
{
	if (m_stream)
	{
		*m_stream << str;
	}
	return *this;
}
Syslogger &Syslogger::operator <<(const Binary& SizeHolder)
{
	if (m_stream)
	{
		*m_stream << std::hex << std::setfill('0');
		for( size_t i = 0; i < (size_t)SizeHolder.m_size; ++i )
			*m_stream << std::setw(2) << int(SizeHolder.m_data[i]) << ' ';
		*m_stream << std::dec;
	}
	return *this;
}

Syslogger::Binary::Binary(const void *data, int size)
	: m_data((const unsigned char *)data), m_size(size)
{
}

Syslogger::Binary::Binary(const char *data, int size)
	 : m_data((const unsigned char *)data), m_size(size)
{
}

Syslogger::Binary::Binary(const unsigned char *data, int size)
	 : m_data((const unsigned char *)data), m_size(size)
{

}

}
