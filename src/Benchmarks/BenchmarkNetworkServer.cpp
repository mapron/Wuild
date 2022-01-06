/*
 * Copyright (C) 2018-2021 Smirnov Vladimir mapron1@gmail.com
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

int main(int argc, char** argv)
{
    using namespace Wuild;
    ConfiguredApplication app(argc, argv, "BenchmarkNetworking");

    TestService                     service;
    TimePoint                       start;
    std::pair<TimePoint, TimePoint> processStart;
    service.startServer([&start, &processStart] {
        start        = TimePoint(true);
        processStart = TimePoint::GetProcessCPUTimes();
        Syslogger(Syslogger::Notice) << "Connected!";
    });
    auto res = ExecAppLoop();

    auto processEnd = TimePoint::GetProcessCPUTimes();
    Syslogger(Syslogger::Notice) << "Taken wall clock     time: " << start.GetElapsedTime().ToProfilingTime();
    Syslogger(Syslogger::Notice) << "Taken user process   time: " << (processEnd.first - processStart.first).ToProfilingTime();
    Syslogger(Syslogger::Notice) << "Taken kernel process time: " << (processEnd.second - processStart.second).ToProfilingTime();
    return res;
}
