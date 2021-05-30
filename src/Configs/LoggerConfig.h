/*
 * Copyright (C) 2017-2021 Smirnov Vladimir mapron1@gmail.com
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

#include "IConfig.h"
#include "Syslogger.h"

namespace Wuild {
class LoggerConfig : public IConfig {
public:
    enum class LogType
    {
        Cout,
        Cerr,
        Printf,
        Syslog,
        Files
    };

    int         m_maxLogLevel = Syslogger::Notice;
    LogType     m_logType     = LogType::Cout;
    std::string m_dir;
    int         m_maxMessagesInFile = 10000;
    int         m_maxFilesInDir     = 5;
    bool        m_outputTimestamp   = false;
    bool        m_outputTimeoffsets = false;
    bool        m_outputLoglevel    = false;

    LoggerConfig(int maxLogLevel = Syslogger::Notice, LogType logType = LogType::Cout)
        : m_maxLogLevel(maxLogLevel)
        , m_logType(logType)
    {}

    bool Validate(std::ostream* errStream = nullptr) const override;
};
}
