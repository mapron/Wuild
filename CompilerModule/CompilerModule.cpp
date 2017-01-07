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

#include "CompilerModule.h"

#include "GccCommandLineParser.h"
#include "MsvcCommandLineParser.h"

#include <CommonTypes.h>
#include <FileUtils.h>
#include <StringUtils.h>
#include <algorithm>

namespace Wuild
{

CompilerModule::CompilerModule()
{
}

void CompilerModule::SetConfig(const ICompilerModule::Config &config)
{
	m_config = config;
}

const ICompilerModule::Config &CompilerModule::GetConfig() const
{
	return m_config;
}

bool CompilerModule::SplitInvocation(const CompilerInvocation & original, CompilerInvocation &preprocessor, CompilerInvocation &compilation)
{
	CompileInfo compiler = CompileInfoById(original.m_id);
	if (!compiler.m_parser)
		return false;
	CompilerInvocation origComplete = CompleteInvocation(original);

	if (origComplete.m_type != CompilerInvocation::InvokeType::Compile)
		return false;

	compiler.m_parser->SetCompilerInvocation(origComplete);
	compiler.m_parser->SetInvokeType(CompilerInvocation::InvokeType::Preprocess);
	compiler.m_parser->RemovePDB();
	preprocessor = compiler.m_parser->GetCompilerInvocation();

	compiler.m_parser->SetCompilerInvocation(origComplete);
	compiler.m_parser->RemovePDB();
	compiler.m_parser->RemovePrepocessorFlags();
	compiler.m_parser->RemoveDependencyFiles();
	compilation = compiler.m_parser->GetCompilerInvocation();

	const std::string srcFilename = origComplete.GetInput(); // we hope  .cpp is coming after -c flag. It's naive.
	const std::string objFilename = origComplete.GetOutput();
	std::string preprocessedFilename = objFilename;
	bool outIsVar = preprocessedFilename[0] == '$'; // ninja hack...
	if (!outIsVar)
	{
		preprocessedFilename = GetPreprocessedPath(srcFilename, objFilename);
		preprocessor.SetOutput(preprocessedFilename);
		compilation.SetInput(preprocessedFilename);
	}

	if (!compiler.m_append.empty())
		compilation.m_args.push_back(compiler.m_append);

	return true;
}

CompilerInvocation CompilerModule::CompleteInvocation(const CompilerInvocation &original)
{
	CompilerInvocation inv;
	inv.m_args.clear();
	inv.m_ignoredArgs = original.m_ignoredArgs;
	for (auto arg : original.m_args)
	{
		StringVector argSplit;
		StringUtils::SplitString(StringUtils::Trim(arg), argSplit, ' ', false, true); //TODO: paths with spaces not supported!
		inv.m_args.insert(inv.m_args.end(), argSplit.begin(), argSplit.end());
	}

	CompileInfo info = CompileInfoById(original.m_id);
	if (info.m_valid)
	{
		inv.m_id = info.m_id;
		inv.m_type = original.m_type;
		inv = info.m_parser->ProcessCompilerInvocation(inv);
	}
	return inv;
}

CompilerInvocation CompilerModule::FilterFlags(const CompilerInvocation &original)
{
	CompileInfo info = CompileInfoById(original.m_id);
	if (info.m_valid)
	{
		 CompilerInvocation flags = CompleteInvocation(original);
		 if (flags.m_type == CompilerInvocation::InvokeType::Preprocess)
		 {
			info.m_parser->SetCompilerInvocation(flags);
			info.m_parser->RemovePDB();
			return info.m_parser->GetCompilerInvocation();
		 }
		 else if (flags.m_type == CompilerInvocation::InvokeType::Compile)
		 {
			info.m_parser->SetCompilerInvocation(flags);
			info.m_parser->RemovePrepocessorFlags();
			info.m_parser->RemoveDependencyFiles();
			info.m_parser->RemovePDB();
			return info.m_parser->GetCompilerInvocation();
		 }
	}
	return original;
}

std::string CompilerModule::GetPreprocessedPath(const std::string & sourcePath,
												const std::string & objectPath) const
{
	FileInfo sourceInfo(sourcePath);
	FileInfo objectInfo(objectPath);

	const std::string preprocessed = objectInfo.GetDir(true) + "pp_" + objectInfo.GetNameWE() + sourceInfo.GetFullExtension();
	return preprocessed;
}

CompilerModule::CompileInfo CompilerModule::CompileInfoById(const CompilerInvocation::Id &id) const
{
	if (id.m_compilerId.empty())
		return CompileInfoByExecutable(id.m_compilerExecutable);

	return CompileInfoByCompilerId(id.m_compilerId);
}

CompilerModule::CompileInfo CompilerModule::CompileInfoByExecutable(const std::string &executable) const
{
   std::string exec = executable;
#ifdef _WIN32
   std::replace(exec.begin(), exec.end(), '\\', '/');
#endif

	CompilerModule::CompileInfo info;
	for (const Config::CompilerUnit & unit : m_config.m_modules)
	{
		if (std::find(unit.m_names.cbegin(), unit.m_names.cend(), exec) != unit.m_names.cend())
		{
			return CompileInfoByUnit(unit);
		}
	}
	return info;
}

CompilerModule::CompileInfo CompilerModule::CompileInfoByCompilerId(const std::string &compilerId) const
{
	CompilerModule::CompileInfo info;
	for (const Config::CompilerUnit & unit : m_config.m_modules)
	{
		if (unit.m_id == compilerId)
			return CompileInfoByUnit(unit);
	}
	return info;
}

CompilerModule::CompileInfo CompilerModule::CompileInfoByUnit(const ICompilerModule::Config::CompilerUnit &unit) const
{
	CompileInfo info;
	info.m_append = unit.m_appendOption;
	info.m_id.m_compilerId = unit.m_id;
	info.m_id.m_compilerExecutable = unit.m_names[0];
	if (unit.m_type == Config::ToolchainType::GCC)
		info.m_parser.reset(new GccCommandLineParser());
	else if (unit.m_type == Config::ToolchainType::MSVC)
		info.m_parser.reset(new MsvcCommandLineParser());
	if (info.m_parser)
		info.m_valid = true;
	return info;
}


}
