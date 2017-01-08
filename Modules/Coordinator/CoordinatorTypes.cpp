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

#include "CoordinatorTypes.h"
#include <sstream>

namespace Wuild
{

std::string CoordinatorWorkerInfo::ToString(bool outputTools, bool outputClients) const
{
	std::ostringstream os;
	os <<  m_connectionHost << ":" << m_connectionPort << " (" << m_workerId << "), threads: " << m_totalThreads;
	if (outputTools)
	{
		os << " Tools: ";
		  for (const std::string & t : m_toolIds)
			  os  << t << ", ";
	}
	if (outputClients)
	{
		os << " Connected: " << m_connectedClients.size();
		for (const ConnectedClientInfo & c : m_connectedClients)
			 os << c.m_clientHost << " (" <<  c.m_clientId << "): use=" << c.m_usedThreads  << ", ";
	}
	return os.str();
}

bool CoordinatorWorkerInfo::EqualIdTo(const CoordinatorWorkerInfo &rh) const
{
	return true
			&& m_workerId == rh.m_workerId
			&& m_connectionHost == rh.m_connectionHost
			&& m_connectionPort == rh.m_connectionPort
			;
}

bool CoordinatorWorkerInfo::ConnectedClientInfo::operator ==(const CoordinatorWorkerInfo::ConnectedClientInfo &rh) const
{
	return true
			&& m_usedThreads == rh.m_usedThreads
			&& m_clientHost == rh.m_clientHost
			&& m_clientId == rh.m_clientHost
			&& m_sessionId == rh.m_sessionId
			;
}

bool CoordinatorWorkerInfo::operator ==(const CoordinatorWorkerInfo &rh) const
{
	return true
			&& EqualIdTo(rh)
			&& m_toolIds == rh.m_toolIds
			&& m_totalThreads == rh.m_totalThreads
			&& m_connectedClients == rh.m_connectedClients
			;
}

std::vector<CoordinatorWorkerInfo *> CoordinatorInfo::Update(const CoordinatorWorkerInfo &newWorker)
{
	return Update(std::deque<CoordinatorWorkerInfo>(1, newWorker));
}

std::vector<CoordinatorWorkerInfo *> CoordinatorInfo::Update(const std::deque<CoordinatorWorkerInfo> &newWorkers)
{
	std::vector<CoordinatorWorkerInfo *> res;

	for (const CoordinatorWorkerInfo &newWorker : newWorkers)
	{
		if (newWorker.m_connectionHost.empty())
			continue;

		bool found = false;
		for (auto & workerInfo : m_workers)
		{
		   if (workerInfo.EqualIdTo(newWorker))
		   {
			   if (workerInfo != newWorker)
			   {
				   res.push_back(&workerInfo);
				   workerInfo = newWorker;
			   }
			   found = true;
			   break;
		   }
		}
		if (!found)
		{
			m_workers.push_back(newWorker);
			res.push_back(&*m_workers.rbegin());
		}
	}
	return res;
}

std::string CoordinatorInfo::ToString(bool outputTools, bool outputClients) const
{
	std::ostringstream os;
	os << "Total workers: " << m_workers.size();
	for (const auto & w : m_workers)
		os << "\n" << w.ToString(outputTools, outputClients);
	return os.str();
}

bool CoordinatorInfo::operator ==(const CoordinatorInfo &rh) const
{
	return m_workers == rh.m_workers;
}

std::string WorkerSessionInfo::ToString(bool full) const
{
	std::ostringstream os;
	os  << "sid=" << m_sessionId
		<< ", client=" << m_clientId
		<< ", tasks=" << m_tasksCount
		   ;
	if (full)
		os << " (err:" << m_failuresCount << "), total execution time:" << m_totalExecutionTime.ToString();
	return os.str();
}


}
