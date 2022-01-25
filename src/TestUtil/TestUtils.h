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
#include <FileUtils.h>
#include <AppUtils.h>
#include <ConfiguredApplication.h>
#include <Application.h>
#include <InvocationRewriter.h>
#include <Syslogger.h>

#include <iostream>
#include <assert.h>

#define TEST_ASSERT(cond) \
    if (!(cond)) { \
        assert(cond); \
        std::cout << "assertion failed: " << #cond << "\n"; \
        return 1; \
    }

namespace Wuild {
class InvocationRewriterStub : public IInvocationRewriter {
    Config m_config;

public:
    InvocationRewriterStub(const Config& config)
        : m_config(config)
    {}

    const Config& GetConfig() const override { return m_config; }

    bool IsCompilerInvocation(const ToolInvocation&) const override { return false; }

    bool SplitInvocation(const ToolInvocation&,
                         ToolInvocation&,
                         ToolInvocation&,
                         std::string*) const override { return false; }

    ToolInvocation     CompleteInvocation(const ToolInvocation& original) const override { return original; }
    ToolInvocation     PrepareRemote(const ToolInvocation& original) const override { return original; }
    ToolInvocation::Id CompleteToolId(const ToolInvocation::Id& original) const override { return original; }
    bool               CheckRemotePossibleForFlags(const ToolInvocation& original) const override { return true; }
    ToolInvocation     FilterFlags(const ToolInvocation& original) const override { return original; }

    std::string GetPreprocessedPath(const std::string&, const std::string& objectPath) const override { return objectPath + ".pp"; }
};

class TestConfiguration {
    TestConfiguration()  = delete;
    ~TestConfiguration() = delete;

public:
    static bool GetTestToolConfig(IInvocationRewriter::Config& settings)
    {
        settings.m_toolIds.push_back("testTool");
        settings.m_tools.resize(1);
        settings.m_tools[0].m_id    = "testTool";
        settings.m_tools[0].m_names = StringVector(1, "test");
        return true;
    }
    static const std::string g_testProgram;

    static IInvocationRewriter::Ptr s_invocationRewriter;

    static void ExitHandler(int code) { std::cout << (code == 0 ? "OK" : "FAIL") << std::endl; }
};

StringVector CreateTestProgramInvocation();

bool CreateInvocationRewriter(ConfiguredApplication& app, bool stub = false);

int HandleTestResult();

int ExecAppLoop();

}
