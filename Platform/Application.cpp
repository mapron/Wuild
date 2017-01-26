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

#include "Application.h"

#include "Syslogger.h"
#include "ThreadUtils.h"
#include "TcpSocket.h"
#include "FileUtils.h"

#include <signal.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <Windows.h>
#endif

namespace Wuild
{
const std::string g_defaultOrganization = "Wuild";
const std::string g_defaultAppname = "test";
std::atomic_bool Application::s_applicationInterruption(false);
std::atomic_int Application::m_exitCode(0);

void Application::SignalHandler(int Signal)
{
	std::string signalName;
	switch (Signal)
	{
		case SIGTERM:
			signalName = "SIGTERM";
			break;

		case SIGINT:
			signalName = "SIGINT";
			break;

#ifndef _WIN32
		case SIGHUP:
			signalName = "SIGHUP";
			break;
#endif
	}
	Syslogger(Syslogger::Warning) << "recieved " << signalName << ".";
	Application::Interrupt();
}

Application::Application()
{
#ifdef _WIN32
	char buf[MAX_PATH + 1];
	int size = GetTempPathA(MAX_PATH, buf);
	m_tmpDir = std::string(buf, size-1);
#elif defined(__APPLE__)
	m_tmpDir = getenv("TMPDIR");
#else
	m_tmpDir = "/tmp";
#endif

	char *envPath;
	char *envDrive;
	if ((envPath = getenv("USERPROFILE"))) {
		m_homeDir = envPath;
	}
	else if ((envDrive = getenv("HOMEDRIVE")) && (envPath = getenv("HOMEPATH")) ) {
		m_homeDir= std::string(envDrive) + envPath;

	}
	if ((envPath = getenv("HOME"))) {
		m_homeDir = envPath;
	}

	m_organization = g_defaultOrganization;
	m_appName = g_defaultAppname;
}

Application & Application::Instance()
{
	static Application instance;
	return instance;
}

void Application::Interrupt(int exitCode)
{
	m_exitCode = exitCode;
	s_applicationInterruption = true;
	TcpSocket::s_applicationInterruption = true;
}

int Application::GetExitCode()
{
	return m_exitCode;
}

std::string Application::GetExecutablePath ()
{
	#ifndef _WIN32
	int len;
	char path[1024];
	char* slash;

	// Read symbolic link /proc/self/exe

	len = readlink("/proc/self/exe", path, sizeof(path));
	if(len == -1)
		return std::string("./");

	path[len] = '\0';

	// Get the directory in the path by stripping exe name

	slash = strrchr(path, '/');
	if(! slash || slash == path)
		return std::string("./");

	*slash = '\0';    // trip slash and exe name

	return std::string(path) + "/";
	#else

	WCHAR szFileName[MAX_PATH];

	GetModuleFileNameW( nullptr, szFileName, MAX_PATH );
	std::wstring w (szFileName);
	std::string s1(w.begin(), w.end() );
	int pos = 0;
	for (size_t i=s1.size()-1;i>0; i--) {
	   if (s1[i] == '\\') {
		  pos = i; break;
	   }
	}

	return std::string(s1.begin(), s1.begin() + pos + 1);

#endif
}

std::string Application::GetTempDir(bool autoCreate) const
{
	std::string dirNameBase= m_tmpDir + "/" + m_organization;
	if (autoCreate)
		FileInfo(dirNameBase).Mkdirs();

	const std::string dirName = dirNameBase + "/" + m_appName + "/";
	if (autoCreate)
		FileInfo(dirName).Mkdirs();
	return dirName;
}

std::string Application::GetAppDataDir(bool autoCreate)
{
	std::string orgPrefix = ".";
#ifndef _WIN32
	const std::string appDataRoot = GetHomeDir();
#else
	orgPrefix = "";
	const std::string appDataRoot = getenv("LOCALAPPDATA");
#endif
	const std::string dirName = appDataRoot + "/" + orgPrefix + m_organization + "/";
	if (autoCreate)
		FileInfo(dirName).Mkdirs();
	return dirName;
}

void Application::SetSignalHandlers()
{
	signal(SIGTERM, SignalHandler);
	signal(SIGINT, SignalHandler);
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SignalHandler);
#endif
}

void Application::WaitForTermination(int terminateAfterMS, int usleepTimeout)
{
	TimePoint start(true);
	while (!s_applicationInterruption)
	{
		if (terminateAfterMS != -1 && start.GetElapsedTime().GetUS() > terminateAfterMS * 1000)
			s_applicationInterruption = true;

		usleep(usleepTimeout);
	}
}

}
