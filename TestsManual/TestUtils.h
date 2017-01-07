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

#pragma once
#include <FileUtils.h>
#include <AppUtils.h>
#include <ConfiguredApplication.h>
#include <Application.h>
#include <CompilerModule.h>
#include <Syslogger.h>

#include <iostream>
#include <assert.h>

#define TEST_ASSERT(cond) if (!(cond)) { assert(0); std::cout << "FAIL\n"; return 1;}

namespace Wuild
{
class CompilerModuleStub : public ICompilerModule
{
	Config m_config;
public:
	CompilerModuleStub(const Config& config) : m_config(config) {}

	void SetConfig(const Config& config) override { m_config = config; }
	const Config& GetConfig() const override { return m_config; }

	bool SplitInvocation(const CompilerInvocation & ,
				 CompilerInvocation &,
				 CompilerInvocation &) { return false;}

	CompilerInvocation CompleteInvocation(const CompilerInvocation & original) { return original; }

	CompilerInvocation FilterFlags(const CompilerInvocation & original)  { return original; }

	std::string GetPreprocessedPath(const std::string &, const std::string & objectPath) const override { return objectPath + ".pp";}

};

class TestConfiguration
{
	TestConfiguration() = delete;
	~TestConfiguration() = delete;

public:

	static bool GetTestToolConfig(ICompilerModule::Config & settings)
	{
		settings.m_toolIds.push_back("testTool");
		settings.m_modules.resize(1);
		settings.m_modules[0].m_id = "testTool";
		settings.m_modules[0].m_names = StringVector(1, "test");
		return true;
	}
	static const std::string g_testProgram;

	static ICompilerModule::Ptr g_compilerModule;

	static bool g_compilerModuleRequired;

	static void ExitHandler (int code){ std::cout << (code == 0 ? "OK" : "FAIL") << std::endl; }
};

StringVector CreateTestProgramInvocation();

bool CreateCompiler(ConfiguredApplication & app, bool stub = false);

int HandleTestResult();

int ExecAppLoop();

}
