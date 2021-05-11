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

#include "LocalExecutor.h"

#include "MsvcEnvironment.h"

#include <subprocess.h>
#include <Syslogger.h>
#include <ThreadUtils.h>

#include <cassert>
#include <utility>
#include <memory>

namespace Wuild
{

LocalExecutor::LocalExecutor(IInvocationRewriter::Ptr invocationRewriter, std::string tempPath, const std::shared_ptr<SubprocessSet> & subprocessSet)
	: m_invocationRewriter(std::move(std::move(invocationRewriter)))
	, m_tempPath(std::move(tempPath))
	, m_subprocs(subprocessSet)
{
}

void LocalExecutor::Start()
{
	m_thread.Exec(std::bind(&LocalExecutor::Quant, this));
}

void LocalExecutor::AddTask(LocalExecutorTask::Ptr task)
{
	Guard guard(m_queueMutex);

	if (!m_thread.IsRunning())
		Start();

	m_taskQueue.push(task);
}

void LocalExecutor::SyncExecTask(LocalExecutorTask::Ptr task)
{
	std::atomic_bool        taskState {false};
	std::condition_variable taskStateCond;
	std::mutex              taskStateMutex;

	auto originalCallback = task->m_callback;
	task->m_callback = [originalCallback, &taskState, &taskStateMutex, &taskStateCond](LocalExecutorResult::Ptr result){
		originalCallback(result);
		std::unique_lock<std::mutex> lock(taskStateMutex);
		taskState = true;
		taskStateCond.notify_one();
	};

	if (task->m_setEnv) // working around subsequent SyncExecTask call. Todo: unlimined SyncExecTask recursion?
		GetToolIdEnvironment(task->m_invocation.m_id.m_toolId);

	AddTask(task);

	std::unique_lock<std::mutex> lock(taskStateMutex);
	taskStateCond.wait(lock, [&taskState]{
		return !!taskState;
	});
}

ILocalExecutor::TaskPair LocalExecutor::SplitTask(LocalExecutorTask::Ptr task, std::string &err)
{
	TaskPair result;
	ToolInvocation pp, cc;
	if (!m_invocationRewriter->SplitInvocation(task->m_invocation, pp, cc))
	{
		err= "Failed to detect CC command.";
		return result;
	}
	LocalExecutorTask::Ptr taskPP(new LocalExecutorTask());
	LocalExecutorTask::Ptr taskCC(new LocalExecutorTask());
	taskPP->m_writeInput = taskCC->m_writeInput = task->m_writeInput;
	taskPP->m_readOutput = taskCC->m_readOutput = task->m_readOutput;
	taskPP->m_invocation = pp;
	taskCC->m_invocation = cc;

	return TaskPair(taskPP, taskCC);
}

StringVector LocalExecutor::GetToolIds() const
{
	return m_invocationRewriter->GetConfig().m_toolIds;
}

void LocalExecutor::SetThreadCount(int threads)
{
	m_maxSubProcesses = threads;
}

size_t LocalExecutor::GetQueueSize() const
{
	Guard guard(m_queueMutex);
	return m_taskQueue.size();
}

LocalExecutor::~LocalExecutor() = default;

void LocalExecutor::CheckSubprocs()
{
	if (!m_subprocs)
		m_subprocs = std::make_shared<SubprocessSet>(false);
}


LocalExecutorTask::Ptr LocalExecutor::GetNextTask()
{
	LocalExecutorTask::Ptr task;
	{
		Guard guard(m_queueMutex);
		if (!m_taskQueue.empty())
		{
			task = m_taskQueue.front();
			m_taskQueue.pop();
		}
	}
	return task;
}

void LocalExecutor::Quant()
{
	CheckSubprocs();

	while (m_subprocs->running_.size() < m_maxSubProcesses)
	{
		auto task = GetNextTask();
		if (task)
		{
			do
			{
				ToolInvocation inv = task->m_invocation;
				inv = m_invocationRewriter->CompleteInvocation(inv);
				StringVector env;
				if (task->m_setEnv)
					env = GetToolIdEnvironment(inv.m_id.m_toolId);

				if (task->m_writeInput)
				{
					FileInfo inputFile (inv.GetInput());
					FileInfo outputFile (inv.GetOutput());

					if (inputFile.GetFullname().empty() || outputFile.GetFullname().empty())
					{
						task->ErrorResult("Failed to extract filenames for " + task->GetShortErrorInfo() );
						break;
					}
					const auto tmpPrefix = m_tempPath + "/" + std::to_string(m_taskId++) + "_";
					task->m_inputFile .SetPath(tmpPrefix + inputFile.GetFullname());
					task->m_outputFile.SetPath(tmpPrefix + outputFile.GetFullname());
					task->m_outputFile.Remove();

					if (! task->m_inputFile.WriteCompressed(task->m_inputData, task->m_compressionInput) )
					{
						break;
					}
					inv.SetInput(task->m_inputFile.GetPath());
					inv.SetOutput(task->m_outputFile.GetPath());
				}

				auto cmd = inv.GetArgsString(true);
				if (cmd.empty())
				{
					task->ErrorResult("Failed to create cmd string for " + task->GetShortErrorInfo() );
					break;
				}
				task->m_invocation = inv;
				task->m_executionStart = TimePoint(true);
				Subprocess * addsubproc = m_subprocs->Add(cmd, false, env);
				if (!addsubproc)
				{
					task->ErrorResult("Failed to execute: " + cmd );
					break;
				}
				m_subprocToTask[addsubproc] = task;
			} while(false);
		}
		else
		{
			break;
		}
	}

	if (!m_subprocs->running_.empty())
	{
		Subprocess* subproc;
		while ((subproc = m_subprocs->NextFinished()) == nullptr) {
		  bool interrupted = m_subprocs->DoWork();
		  if (interrupted)
			return;
		}

		LocalExecutorResult::Ptr result(new LocalExecutorResult());
		result->m_result = subproc->Finish() == ExitSuccess;
		result->m_stdOut = subproc->GetOutput();

		auto taskIter = m_subprocToTask.find(subproc);
		assert(taskIter != m_subprocToTask.end());
		LocalExecutorTask::Ptr task = taskIter->second;
		m_subprocToTask.erase(taskIter);
		delete subproc;

		result->m_executionTime = task->m_executionStart.GetElapsedTime();
		const auto & executableName = task->m_invocation.m_id.m_toolExecutable;
		if (result->m_stdOut.size() < 1000
				&& result->m_stdOut.find_first_of('\n') == result->m_stdOut.size()-1
				&& executableName.find("cl.exe") != std::string::npos)
		{   // cl.exe always outputs input name to stderr.
			result->m_stdOut.clear();
		}

		std::ostringstream compressionInfo;
		if (result->m_result && task->m_readOutput)
		{
			result->m_result = task->m_outputFile.ReadCompressed(result->m_outputData, task->m_compressionOutput);
			compressionInfo << " [" << task->m_outputFile.GetFileSize() << " / " << result->m_outputData.size() << "]";

			if (!result->m_result)
				result->m_stdOut = "Failed to read file " + task->m_outputFile.GetPath();
		}
		if (!task->m_outputFile.GetPath().empty())
			Syslogger(Syslogger::Notice) << task->GetShortErrorInfo() << " -> " << task->m_outputFile.GetPath() << compressionInfo.str();

		assert(bool(task->m_callback));
		task->m_callback(result);
	}
}

const StringVector & LocalExecutor::GetToolIdEnvironment(const std::string & toolId)
{
	auto it = m_toolIdEnvironment.find(toolId);
	if (it != m_toolIdEnvironment.end())
		return it->second;

	StringVector env;
	for (const InvocationRewriterConfig::Tool & tool : m_invocationRewriter->GetConfig().m_tools)
	{
		if (tool.m_id == toolId && !tool.m_envCommand.empty())
		{
			env = ExtractVsVars(tool.m_envCommand, *this);
		}
	}
	auto newit = m_toolIdEnvironment.insert(std::make_pair(toolId, env));
	return newit.first->second;
}

}
