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

#include <string>
#include <map>
#include <vector>
#include <atomic>

namespace Wuild {
/// Class provides some environment checking and signals handling .
class Application {
    /// Set to true when user interrupts application. Changed by signal handler.
    static std::atomic_bool s_applicationInterruption;
    std::string             m_organization;
    std::string             m_appName;
    std::string             m_homeDir;
    std::string             m_tmpDir;
    static std::atomic_int  m_exitCode;

    static void SignalHandler(int Signal);
    Application();

public:
    static Application& Instance();

    /// Check application was interrupted (by signal or another exit condition)
    static bool IsInterrupted() { return s_applicationInterruption; }

    /// Explicitly interrupt application. exitCode becomes main() exit code.
    static void Interrupt(int exitCode = 0);

    /// Get current application exit code
    static int GetExitCode();

    /// Return directory holding application binary.
    std::string GetExecutablePath();

    /// Organization to create app folder (default Wuild)
    void SetOrganizationName(const std::string& name) { m_organization = name; }

    /// Application name (to separate logs directories)
    void SetAppName(const std::string& name) { m_appName = name; }

    /// Return system temporary dir. Automatically create subfolder if "autoCreate" is true.
    std::string GetTempDir(bool autoCreate = true) const;

    /// Return user home directory
    std::string GetHomeDir() const { return m_homeDir; }

    /// Return application data folder. %LOCALAPPDATA%/organization on windows, ~/.organization on Unix.
    std::string GetAppDataDir(bool autoCreate = true);

    /// Sets up signal hadler for application interruption.
    void SetSignalHandlers();

    /// Runs infinite loop which will stop after interruption.
    void WaitForTermination(int terminateAfterMS = -1, int usleepTimeout = 1000);
};

}
