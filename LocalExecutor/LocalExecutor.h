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
#include <ILocalExecutor.h>
#include <ICompilerModule.h>
#include <ThreadLoop.h>

#include <queue>
#include <map>
#include <atomic>
#include <mutex>

// ninja classes.
struct Subprocess;
struct SubprocessSet;

namespace Wuild
{
/// Executes command on local host and notifies caller when task finished.
///
/// Uses ninja's SubprocessSet.
class LocalExecutor : public ILocalExecutor
{
	LocalExecutor(ICompilerModule::Ptr compiler, const std::string & tempPath);
public:
	static Ptr Create(ICompilerModule::Ptr compiler, const std::string & tempPath)
	{ return Ptr(new LocalExecutor(compiler, tempPath)); }

public:
	void AddTask(LocalExecutorTask::Ptr task) override;
	TaskPair SplitTask(LocalExecutorTask::Ptr compiler, std::string & err) override;
	StringVector GetToolIds() const override;
	void SetWorkersCount(int workers) override;

	~LocalExecutor();

private:
	void Start();
	void CheckSubprocs();
	LocalExecutorTask::Ptr GetNextTask();
	void Quant();

	size_t m_maxWorkers = 1;
	size_t m_taskId = 0;
	std::mutex m_queueMutex;
	using Guard = std::lock_guard<std::mutex>;
	std::queue<LocalExecutorTask::Ptr> m_taskQueue;

	std::shared_ptr<ICompilerModule> m_compiler;
	std::string m_tempPath;
	std::unique_ptr<SubprocessSet> m_subprocs;
	std::map<Subprocess*, LocalExecutorTask::Ptr> m_subprocToTask;
	ThreadLoop m_thread;
};

}
