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

namespace Wuild
{
/// Logging implementation
class ILoggerBackend
{
public:
	virtual ~ILoggerBackend() = default;

	/// Returns true if log should be flushed. Syslogger::Emerg <= loglevel <=  Syslogger::Debug
	virtual bool LogEnabled(int logLevel) const = 0;

	/// Outputs log message to log backend.
	virtual void FlushMessage(const std::string & message, int logLevel) const = 0;
};

/// Logger to standard output or another backend.
class Syslogger
{
public:
	enum LogLevel { Emerg, Alert, Crit, Err, Warning, Notice, Info, Debug };

	 /// Change default logging behaviour.
	static void SetLoggerBackend(std::unique_ptr<ILoggerBackend> && backend);
	static void SetDeferredMode(bool deferred);
	static bool IsLogLevelEnabled(int logLevel);

	/// Convenience wrapper for blobs. When outputting to Syslogger, output will be HEX-formatted.
	class Binary
	{
		friend class Syslogger;
		const unsigned char* m_data;
		const size_t m_size;
		const size_t m_outputMax;
	public:
		Binary (const void* data, size_t size, size_t outputMax = size_t(-1));
		Binary (const char* data, size_t size, size_t outputMax = size_t(-1));
		Binary (const unsigned char* data, size_t size, size_t outputMax = size_t(-1));
	};
	/// Creates logger. If loglevel is flushable by backend, stream will be created.
	Syslogger (int logLevel = Syslogger::Debug);

	/// Convenience constructor for outputting context. Context string will be enclosed in braces, if not empty.
	Syslogger (const std::string & context, int logLevel = Syslogger::Debug);

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
	int m_logLevel{};

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
