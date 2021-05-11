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

#include "Application.h"
#include "FileUtils.h"
#include "TimePoint.h"

#include <cstdio>
#include <iostream>
#include <algorithm>
#include <utility>

namespace Wuild {

LoggerBackendFiles::LoggerBackendFiles(int         maxLogLevel,
                                       bool        outputLoglevel,
                                       bool        outputTimestamp,
                                       bool        outputTimeoffsets,
                                       int         maxFilesInDir,
                                       int         maxMessagesInFile,
                                       std::string dir)
    : AbstractLoggerBackend(maxLogLevel, outputLoglevel, outputTimestamp, outputTimeoffsets, true)
    , m_dir(std::move(dir))
    , m_maxFilesInDir(maxFilesInDir)
    , m_maxMessagesInFile(maxMessagesInFile)
{
    FileInfo(m_dir).Mkdirs();
}

LoggerBackendFiles::~LoggerBackendFiles()
{
    CloseFile();
}

void LoggerBackendFiles::FlushMessageInternal(const std::string& message, int) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_currentFile)
        OpenNextFile();

    if (!m_currentFile)
        return;

    bool result = fwrite(message.c_str(), message.size(), 1, m_currentFile) > 0;
    if (!result)
        std::cerr << "fwrite \'" << message << "\' failed! \n";

    fflush(m_currentFile);

    if (++m_counter > m_maxMessagesInFile) {
        m_counter = 0;
        CloseFile();
    }
}

void LoggerBackendFiles::OpenNextFile() const
{
    CloseFile();

    CleanupDir();

    std::string timeString = TimePoint(true).ToString(true, true);
    std::replace(timeString.begin(), timeString.end(), ' ', '_');
    timeString.erase(std::remove(timeString.begin(), timeString.end(), '-'), timeString.end());
    timeString.erase(std::remove(timeString.begin(), timeString.end(), ':'), timeString.end());

    auto filename = m_dir + "/" + timeString + ".log";
    m_currentFile = fopen(filename.c_str(), "wb");
    if (!m_currentFile)
        std::cerr << "Failed to open:" << filename << std::endl;
}

void LoggerBackendFiles::CloseFile() const
{
    if (m_currentFile) {
        fclose(m_currentFile);
    }
    m_currentFile = nullptr;
}

void LoggerBackendFiles::CleanupDir() const
{
    StringVector contents = FileInfo(m_dir).GetDirFiles();
    if (contents.size() > m_maxFilesInDir) {
        contents.erase(contents.end() - m_maxFilesInDir, contents.end());
        for (const auto& file : contents) {
            FileInfo(m_dir + "/" + file).Remove();
        }
    }
}

}
