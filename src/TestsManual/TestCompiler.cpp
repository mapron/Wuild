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

#include "TestUtils.h"

#include <LocalExecutor.h>
#include <Syslogger.h>
#include <Application.h>
#include <FileUtils.h>

#include <iostream>

/*
 * Test Wuild compiler configuration.
 * Arguments is not required, test creates test invocation for first toolchain in config file.
 */
int main(int argc, char** argv)
{
    using namespace Wuild;
    ConfiguredApplication app(argc, argv, "TestCompiler");
    if (!CreateInvocationRewriter(app))
        return 1;

    //Syslogger() << app.DumpAllConfigValues();
    Syslogger() << app.m_invocationRewriterConfig.Dump();

    const auto args = CreateTestProgramInvocation();

    auto localExecutor = LocalExecutor::Create(TestConfiguration::s_invocationRewriter, app.m_tempDir);

    std::string            err;
    LocalExecutorTask::Ptr original(new LocalExecutorTask());
    original->m_readOutput = original->m_writeInput = false;
    original->m_invocation                          = ToolInvocation(args).SetExecutable(TestConfiguration::s_invocationConfig.GetFirstToolName());
    auto tasks                                      = localExecutor->SplitTask(original, err);
    if (!tasks.first) {
        Syslogger(Syslogger::Err) << err;
        return 1;
    }
    LocalExecutorTask::Ptr taskPP = tasks.first;
    LocalExecutorTask::Ptr taskCC = tasks.second;

    taskCC->m_callback = [](Wuild::LocalExecutorResult::Ptr result) {
        Syslogger() << "CC result = " << result->m_result;
        std::replace(result->m_stdOut.begin(), result->m_stdOut.end(), '\r', ' ');
        if (result->m_result && !result->m_stdOut.empty())
            Syslogger(Syslogger::Notice) << result->m_stdOut;
        if (!result->m_result)
            Syslogger(Syslogger::Err) << result->m_stdOut;

        Application::Interrupt(1 - result->m_result);
    };

    taskPP->m_callback = [&localExecutor, taskCC](LocalExecutorResult::Ptr localResult) {
        if (!localResult->m_result) {
            Syslogger(Syslogger::Err) << "Preprocess failed: \n"
                                      << localResult->m_stdOut;
            Application::Interrupt(1);
            return;
        }
        localExecutor->AddTask(taskCC);
    };
    localExecutor->AddTask(taskPP);

    return ExecAppLoop(TestConfiguration::ExitHandler);
}
