/*
 * Copyright (C) 2018 Smirnov Vladimir mapron1@gmail.com
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
#include "BenchmarkUtils.h"

#include <ArgStorage.h>

int main(int argc, char** argv)
{
	using namespace Wuild;
	ArgStorage argStorage(argc, argv);
	ConfiguredApplication app(argStorage.GetConfigValues(), "BenchmarkNetworking");
	auto args = argStorage.GetArgs();
	if (args.size() < 1)
	{
		Syslogger(Syslogger::Err) << "Usage: <server ip>";
		return 1;
	}

	TestService service;
	service.startClient(args[0]);
	TimePoint start(true);
	for (int i = 0; i< 50; i++)
		service.sendFile(1000000);
	service.waitForReplies();
	Syslogger(Syslogger::Notice) << "Taken time:" << start.GetElapsedTime().ToProfilingTime();

	return 0;
}
