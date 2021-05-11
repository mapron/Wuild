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

#include "MsvcEnvironment.h"

#include <StringUtils.h>
#include <Syslogger.h>

#include <algorithm>

namespace Wuild {

StringVector ExtractVsVars(const std::string& vsvarsCommand, ILocalExecutor& executor)
{
    StringVector result;

    auto varsCheckTask          = std::make_shared<LocalExecutorTask>();
    varsCheckTask->m_readOutput = varsCheckTask->m_writeInput = false;
    varsCheckTask->m_setEnv                                   = false;
    varsCheckTask->m_invocation.m_id.m_toolExecutable         = "cmd.exe";
    std::string cmd                                           = "/C \"" + vsvarsCommand + "\" && set";
    varsCheckTask->m_invocation.SetArgsString(cmd);

    varsCheckTask->m_callback = [&result](LocalExecutorResult::Ptr taskResult) {
        if (taskResult->m_result) {
            StringUtils::SplitString(taskResult->m_stdOut, result, '\n', true, true);
        } else {
            std::replace(taskResult->m_stdOut.begin(), taskResult->m_stdOut.end(), '\r', ' ');
            Syslogger(Syslogger::Notice) << "ERROR: " << taskResult->m_stdOut;
        }
    };
    executor.SyncExecTask(varsCheckTask);

    return result;
}

}
