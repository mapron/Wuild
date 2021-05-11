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

#include "TestUtils.h"

namespace Wuild {
const std::string TestConfiguration::g_testProgram = "#include<iostream> \n"
                                                     "int main() { std::cout << \"OK\\n\"; return 0; } \n";

IInvocationRewriter::Ptr TestConfiguration::s_invocationRewriter;

StringVector CreateTestProgramInvocation()
{
    std::string     tempDir     = Application::Instance().GetTempDir();
    std::string     testFileCpp = tempDir + "/test.cpp";
    ByteArrayHolder fileData;
    fileData.ref().insert(fileData.ref().begin(), TestConfiguration::g_testProgram.cbegin(), TestConfiguration::g_testProgram.cend());
    FileInfo(testFileCpp).WriteFile(fileData);

    StringVector args;
    args.push_back("-c");
    args.push_back(testFileCpp);
    args.push_back("-o");
    args.push_back(tempDir + "/test.o");

#ifdef __APPLE__
    args.push_back("-isysroot");
    args.push_back("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk");
#endif
    return args;
}

int HandleTestResult()
{
    std::cout << (Application::GetExitCode() == 0 ? "OK" : "FAIL") << std::endl;
    return Application::GetExitCode();
}

bool CreateInvocationRewriter(ConfiguredApplication& app, bool stub)
{
    if (!stub) {
        IInvocationRewriter::Config config;
        if (!app.GetInvocationRewriterConfig(config))
            return false;

        TestConfiguration::s_invocationRewriter = InvocationRewriter::Create(config);
    } else {
        IInvocationRewriter::Config config;
        TestConfiguration::GetTestToolConfig(config);
        TestConfiguration::s_invocationRewriter.reset(new InvocationRewriterStub(config));
    }
    return true;
}

}
