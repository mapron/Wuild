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

#include <subprocess.h>
#include <Syslogger.h>
#include <ThreadUtils.h>

#include <assert.h>

namespace Wuild
{

LocalExecutor::LocalExecutor(IInvocationRewriter::Ptr invocationRewriter, const std::string &tempPath)
	: m_invocationRewriter(invocationRewriter)
	, m_tempPath(tempPath)
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

LocalExecutor::~LocalExecutor()
{
}

void LocalExecutor::CheckSubprocs()
{
	if (!m_subprocs)
		m_subprocs.reset(new SubprocessSet(false));
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
	if (!m_subprocs->running_.empty())
	{
		 m_subprocs->DoWork();
	}

	Subprocess* subproc;
	while ((subproc = m_subprocs->NextFinished()))
	{
		LocalExecutorResult::Ptr result(new LocalExecutorResult());
		result->m_result = subproc->Finish() == ExitSuccess;
		result->m_stdOut = subproc->GetOutput();

		auto taskIter = m_subprocToTask.find(subproc);
		assert(taskIter != m_subprocToTask.end());
		LocalExecutorTask::Ptr task = taskIter->second;
		m_subprocToTask.erase(taskIter);
		delete subproc;

		result->m_executionTime = task->m_executionStart.GetElapsedTime();

		std::ostringstream compressionInfo;
		if (result->m_result && task->m_readOutput)
		{
			result->m_result = task->m_outputFile.ReadGzipped(result->m_outputData);
			compressionInfo << " [" << task->m_outputFile.FileSize() << " / " << result->m_outputData.size() << "]";

			if (!result->m_result)
				result->m_stdOut = "Failed to read file " + task->m_outputFile.GetPath();
		}
		Syslogger(LOG_NOTICE) << task->GetShortErrorInfo() << " -> " << task->m_outputFile.GetPath() << compressionInfo.str();

		assert(bool(task->m_callback));
		task->m_callback(result);
	}

	if (m_subprocs->running_.size() < m_maxSubProcesses)
	{
		auto task = GetNextTask();
		if (task)
		{
			do
			{
				ToolInvocation inv = task->m_invocation;
				inv = m_invocationRewriter->CompleteInvocation(inv);

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

					if (! task->m_inputFile.WriteGzipped(task->m_inputData) )
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
				task->m_executionStart = TimePoint(true);
				Subprocess * addsubproc = m_subprocs->Add(cmd);
				if (!addsubproc)
				{
					task->ErrorResult("Failed to execute: " + cmd );
					break;
				}
				m_subprocToTask[addsubproc] = task;
			} while(false);
		}
	}

}

}
