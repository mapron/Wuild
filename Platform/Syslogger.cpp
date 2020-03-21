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
#include <algorithm>
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <utility>

namespace Wuild
{

class DeferredBackendProxy : public ILoggerBackend
{
public:
	explicit DeferredBackendProxy(std::unique_ptr<ILoggerBackend> && backend)
	{
		SetLoggerBackend(std::move(backend));
	}

	void SetLoggerBackend(std::unique_ptr<ILoggerBackend> && backend)
	{
		m_loggerBackend = std::move(backend);
	}

	void SetDeferredMode(bool deferred)
	{
		m_deferred = deferred;
		if (!deferred)
		{
			const std::unique_lock<std::mutex> guard(m_queueMutex);
			while (!m_messagesQueue.empty())
			{
				const auto & message = m_messagesQueue.front();
				m_loggerBackend->FlushMessage(message.first, message.second);
				m_messagesQueue.pop();
			}
		}
	}

	bool LogEnabled(int logLevel) const override
	{
		return m_loggerBackend->LogEnabled(logLevel);
	}

	void FlushMessage(const std::string & message, int logLevel) const override
	{
		if (m_deferred)
		{
			const std::unique_lock<std::mutex> guard(m_queueMutex);
			m_messagesQueue.push(std::make_pair(message, logLevel));
		}
		else
		{
			m_loggerBackend->FlushMessage(message, logLevel);
		}
	}

private:
	std::unique_ptr<ILoggerBackend> m_loggerBackend;
	std::atomic_bool m_deferred {false};
	mutable std::mutex m_queueMutex;
	mutable std::queue<std::pair<std::string, int>> m_messagesQueue;
};

static DeferredBackendProxy g_loggerBackendProxy(std::make_unique<LoggerBackendConsole>(Syslogger::Notice, false, false, false, LoggerBackendConsole::Type::Cout));

void Syslogger::SetLoggerBackend(std::unique_ptr<ILoggerBackend> && backend)
{
	g_loggerBackendProxy.SetLoggerBackend(std::move(backend));
}

void Syslogger::SetDeferredMode(bool deferred)
{
	g_loggerBackendProxy.SetDeferredMode(deferred);
}

bool Syslogger::IsLogLevelEnabled(int logLevel)
{
	return g_loggerBackendProxy.LogEnabled(logLevel);
}

Syslogger::Syslogger(int logLevel)
	: m_logLevel(logLevel)
{
	if (g_loggerBackendProxy.LogEnabled(logLevel))
		m_stream = std::make_unique<std::ostringstream>();
}

Syslogger::Syslogger(const std::string &context, int logLevel)
{
	if (g_loggerBackendProxy.LogEnabled(logLevel))
	{
		m_stream = std::make_unique<std::ostringstream>();
		if (!context.empty())
		   *m_stream << "{" << context << "} ";
	}
}

Syslogger::~Syslogger()
{
	if (m_stream)
		g_loggerBackendProxy.FlushMessage(m_stream->str(), m_logLevel);
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
		*m_stream << "[" << SizeHolder.m_size << "] ";
		*m_stream << std::hex << std::setfill('0');
		const size_t outputSize = std::min(SizeHolder.m_size, SizeHolder.m_outputMax);

		if (SizeHolder.m_size <= SizeHolder.m_outputMax)
		{
			for( size_t i = 0; i < outputSize; ++i )
				*m_stream << std::setw(2) << int(SizeHolder.m_data[i]) << ' ';
		}
		else
		{
			for( size_t i = 0; i < outputSize / 2; ++i )
				*m_stream << std::setw(2) << int(SizeHolder.m_data[i]) << ' ';

			*m_stream << "... ";

			for( size_t i = 0; i < outputSize / 2; ++i )
				*m_stream << std::setw(2) << int(SizeHolder.m_data[SizeHolder.m_size - outputSize / 2 + i]) << ' ';
		}
		*m_stream << std::dec;
	}
	return *this;
}

Syslogger::Binary::Binary(const void *data, size_t size, size_t outputMax)
	: m_data((const unsigned char *)data), m_size(size), m_outputMax(outputMax)
{
}

Syslogger::Binary::Binary(const char *data, size_t size, size_t outputMax)
	 : m_data((const unsigned char *)data), m_size(size), m_outputMax(outputMax)
{
}

Syslogger::Binary::Binary(const unsigned char *data, size_t size, size_t outputMax)
	 : m_data((const unsigned char *)data), m_size(size), m_outputMax(outputMax)
{

}

}
