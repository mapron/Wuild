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
#include <vector>
#include <string>
#include <set>

struct Edge;

class IRemoteExecutor
{
public:
	virtual ~IRemoteExecutor() = default;

	virtual bool PreprocessCode(const std::vector<std::string> & originalRule,
								const std::vector<std::string> & ignoredArgs,
								std::string & toolId,
								std::vector<std::string> & preprocessRule,
								std::vector<std::string> & compileRule) const = 0;

	virtual std::string GetPreprocessedPath(const std::string & sourcePath, const std::string & objectPath) const = 0;

	virtual std::string FilterPreprocessorFlags(const std::string & toolId, const std::string & flags) const = 0;
	virtual std::string FilterCompilerFlags(const std::string & toolId, const std::string & flags) const = 0;

	virtual void RunIfNeeded(const std::vector<std::string> & toolIds) = 0;
	virtual int GetMinimalRemoteTasks() const = 0;
	virtual void SleepSome() const = 0;

	virtual bool CanRunMore() = 0;
	virtual bool StartCommand(Edge* userData, const std::string & command) = 0;

	/// The result of waiting for a command.
	struct Result {
	  Result() = default;
	  Result(Edge* u, bool s, const std::string & o) : userData(u), status(s), output(o) {}
	  Edge* userData = nullptr;
	  bool status = false;
	  std::string output;
	  bool success() const { return status; }
	};
	/// Wait for a command to complete, or return false if interrupted.
	virtual bool WaitForCommand(Result* result) = 0;

	virtual void Abort() = 0;
	virtual std::set<Edge*> GetActiveEdges() = 0;
};
