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

#ifdef __linux__  // ensure Syslogger::Emerg* defines is first.
#include <sys/syslog.h>
#endif

#include "Syslogger.h"

#include <mutex>
#include <iostream>

namespace Wuild {

class AbstractLoggerBackend : public ILoggerBackend
{
public:
	AbstractLoggerBackend(int maxLogLevel,
						  bool outputLoglevel,
						  bool outputTimestamp,
						  bool outputTimeoffsets,
						  bool appendEndl)
		: m_maxLogLevel(maxLogLevel)
		, m_outputLoglevel(outputLoglevel)
		, m_outputTimestamp(outputTimestamp)
		, m_outputTimeoffsets(outputTimeoffsets)
		, m_appendEndl(appendEndl)
	{
		m_startTime = TimePoint(true);
	}

	bool LogEnabled(int logLevel) const override { return logLevel <= m_maxLogLevel; }

	void FlushMessage(const std::string & message, int logLevel) const override
	{
		std::ostringstream os;
		if (m_outputLoglevel)
		{
			os << LogLevelString(logLevel) << " ";
		}
		if (m_outputTimestamp)
		{
			os << "[" << TimePoint(true).ToString() << "] ";
		}

		if (m_outputTimeoffsets)
		{
			TimePoint now(true);
			const auto fromStart = (now-m_startTime).GetUS();
			const auto fromPrev = (now-m_prevTime).GetUS();
			os << "[" << fromStart << ", +" << fromPrev<< "] ";
			m_prevTime = now;
		}
		os << message;
		if (m_appendEndl)
			os << std::endl;
		FlushMessageInternal(os.str(), logLevel);
	}

	virtual void FlushMessageInternal(const std::string & message, int logLevel) const = 0;

protected:
	std::string LogLevelString(int loglevel) const
	{
		switch (loglevel)
		{
		case Syslogger::Emerg    : return "EMERG";
		case Syslogger::Alert    : return "ALERT";
		case Syslogger::Crit     : return "CRIT ";
		case Syslogger::Err		 : return "ERROR";
		case Syslogger::Warning  : return "WARNI";
		case Syslogger::Notice   : return "NOTIC";
		case Syslogger::Info     : return "INFO ";
		case Syslogger::Debug    : return "DEBUG";
		default:
			return "?????";
		}
	}
private:
	TimePoint m_startTime;
	mutable TimePoint m_prevTime;
	const int m_maxLogLevel;
	const bool m_outputLoglevel;
	const bool m_outputTimestamp;
	const bool m_outputTimeoffsets;
	const bool m_appendEndl;
};

class LoggerBackendConsole : public AbstractLoggerBackend
{
public:	
	enum class Type { Cout, Cerr, Printf };
	
public:
    LoggerBackendConsole(int maxLogLevel,
					  bool outputLoglevel,
					  bool outputTimestamp,
                      bool outputTimeoffsets,
                      Type type)
        : AbstractLoggerBackend(maxLogLevel, outputLoglevel, outputTimestamp, outputTimeoffsets, false)
        , m_type(type) { }
	void FlushMessageInternal(const std::string & message, int ) const override
	{
		if (m_type == Type::Printf)
			printf("%s\n", message.c_str());
        else if (m_type == Type::Cerr)
			std::cerr << message << std::endl << std::flush;
        else
			std::cout << message << std::endl << std::flush;
	}
private:
	const Type m_type;
};
#ifdef __linux__
class LoggerBackendSyslog : public AbstractLoggerBackend
{
public:
	LoggerBackendSyslog(int maxLogLevel,
						bool outputLoglevel,
						bool outputTimestamp,
						bool outputTimeoffsets)
		: AbstractLoggerBackend(maxLogLevel, outputLoglevel, outputTimestamp, outputTimeoffsets, true)
	{ openlog(nullptr, LOG_CONS | LOG_NDELAY | LOG_PERROR, LOG_USER) ; }
	~LoggerBackendSyslog() { closelog(); }
	void FlushMessageInternal(const std::string & message, int logLevel) const override
	{
	   syslog(logLevel, "%s", message.c_str());
	}
};
#endif

class LoggerBackendFiles : public AbstractLoggerBackend
{
public:
	LoggerBackendFiles(int maxLogLevel,
					   bool outputLoglevel,
					   bool outputTimestamp,
					   bool outputTimeoffsets,
					   int maxFilesInDir,
					   int maxMessagesInFile,
					   std::string  dir);
	~LoggerBackendFiles();
	void FlushMessageInternal(const std::string & message, int ) const override;

private:
	void OpenNextFile() const;
	void CloseFile() const;
	void CleanupDir() const;
	const std::string m_dir;
	const int m_maxFilesInDir;
	const int m_maxMessagesInFile;
	mutable FILE * m_currentFile = nullptr;
	mutable int m_counter = 0;
	mutable std::mutex m_mutex;
};

}
