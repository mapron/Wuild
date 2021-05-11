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

#include "LocalExecutorTask.h"

namespace Wuild
{
/// Interface for execution tasks on local host.
class ILocalExecutor
{
public:
	using Ptr = std::shared_ptr<ILocalExecutor>;
	using TaskPair = std::pair<LocalExecutorTask::Ptr, LocalExecutorTask::Ptr>;

	virtual ~ILocalExecutor() = default;

	/// Schedule task for execution. task contains callback to call when finished. 
	virtual void AddTask(LocalExecutorTask::Ptr task) = 0;
	
	/// Caller thread will blocked until task finished. Precondition: queue must be empty.
	virtual void SyncExecTask(LocalExecutorTask::Ptr task) = 0;

	/// Try to split task to preprocessing and compilation tasks. Returns empty TaskPair on fail.
	virtual TaskPair SplitTask(LocalExecutorTask::Ptr task, std::string & err) = 0;

	/// Returns tools ids available for execution.
	virtual StringVector GetToolIds() const = 0;

	/// Sets maximal process count.
	virtual void SetThreadCount(int threads) = 0;

	/// Queued tasks count.
	virtual size_t GetQueueSize() const = 0;
};
}
