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

#pragma once
#include "TimePoint.h"

#include <string>
#include <vector>
#include <memory>
#include <deque>
#include <sstream>
#include <map>

#ifndef LOG_EMERG
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#endif

namespace Wuild
{
/// Logging implementation
class ILoggerBackend
{
public:
	virtual ~ILoggerBackend() = default;

	/// Returns true if log should be flushed. LOG_EMERG <= loglevel <=  LOG_DEBUG
	virtual bool LogEnabled(int logLevel) const = 0;

	/// Outputs log message to log backend.
	virtual void FlushMessage(const std::string & message, int logLevel) const = 0;
};

/// Logger to standard output or another backend.
class Syslogger
{
public:

	 /// Change default logging behaviour.
	static void SetLoggerBackend(std::unique_ptr<ILoggerBackend> && backend);
	static bool IsLogLevelEnabled(int logLevel);

	/// Convenience wrapper for blobs. When outputting to Syslogger, output will be HEX-formatted.
	class Binary
	{
		friend class Syslogger;
		const unsigned char* m_data;
		const int m_size;
	public:
		Binary (const void* data, int size);
		Binary (const char* data, int size);
		Binary (const unsigned char* data, int size);
	};
	/// Creates logger. If loglevel is flushable by backend, stream will be created.
	Syslogger (int logLevel = LOG_DEBUG);

	/// Convenience constructor for outputting context. Context string will be enclosed in braces, if not empty.
	Syslogger (const std::string & context, int logLevel = LOG_DEBUG);

	/// Flushes all output to backend.
	~Syslogger();

	/// Returns true if logger is active and Stream exists.
	operator bool() const;

	Syslogger& operator << (const char *str);
	Syslogger& operator << (const Binary& SizeHolder);
	template <class Data>  Syslogger& operator << (const Data& D) { if (m_stream) *m_stream << D; return *this;}
	template <class Data>  Syslogger& operator << (const std::vector<Data>& D);
	template <class Data>  Syslogger& operator << (const std::deque<Data>& D);

private:
	std::unique_ptr<std::ostringstream> m_stream;   //!< Stream used for buffer formatting
	int m_logLevel;

	void flush();

	Syslogger(const Syslogger&) = delete;
	void operator = (const Syslogger&) = delete;
	// move operations doesn't look evil.
};


template <class Data>
Syslogger& Syslogger::operator << (const std::vector<Data>& D)
{
	if (m_stream)
	{
		for (const auto & d : D)
			*m_stream << d << ' ';
	}
	return *this;
}

template <class Data>
Syslogger& Syslogger::operator << (const std::deque<Data>& D)
{
	if (m_stream)
	{
		for (const auto & d : D)
			*m_stream << d << ' ';
	}
	return *this;
}

}
