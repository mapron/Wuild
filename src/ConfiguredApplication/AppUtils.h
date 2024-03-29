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
#include "ConfiguredApplication.h"

#include <Application.h>
#include <InvocationToolProvider.h>

#include <functional>

namespace Wuild {
inline IInvocationToolProvider::Ptr CheckedCreateInvocationToolProvider(ConfiguredApplication& application)
{
    IInvocationToolProvider::Config config;
    if (!application.GetInvocationToolConfig(config, false))
        return nullptr;
    return InvocationToolProvider::Create(config);
}

inline int ExecAppLoop(std::function<void(int)> exitCodeHandler = std::function<void(int)>())
{
    Application::Instance().SetSignalHandlers();
    Application::Instance().WaitForTermination();
    if (exitCodeHandler)
        exitCodeHandler(Application::GetExitCode());
    return Application::GetExitCode();
}

}
