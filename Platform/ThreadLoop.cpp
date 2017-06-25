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

#include "ThreadLoop.h"

#include "Application.h"
#include "ThreadUtils.h"
#include "Syslogger.h"

#include <thread>
#include <atomic>
#include <assert.h>

namespace Wuild
{

class ThreadLoopImpl
{
public:
	std::thread m_thread;
	std::atomic_bool m_condition {false};
};

ThreadLoop::ThreadLoop()
	: m_impl(new ThreadLoopImpl)
{

}

ThreadLoop::ThreadLoop(ThreadLoop &&rh)
	: m_impl(std::move(rh.m_impl))
{
}

ThreadLoop::~ThreadLoop()
{
	Stop();
}

bool ThreadLoop::IsRunning() const
{
	return m_impl->m_condition;
}

void ThreadLoop::Exec(ThreadLoop::QuantFunction quant, int64_t sleepUS)
{
	Stop();
	m_impl->m_condition = true;
	m_impl->m_thread = std::thread([quant, this, sleepUS]{
		try
		{
			while (m_impl->m_condition && !Application::IsInterrupted())
			{
				quant();
				usleep(sleepUS);
			}
		}
		catch (std::exception& ex)
		{
			Syslogger(Syslogger::Err) << "std::exception caught in ThreadLoop::Exec " << ex.what();
		}
		catch (...)
		{
			Syslogger(Syslogger::Crit) << "Exception caught in ThreadLoop::mainCycle.";
		}

	});
}

void ThreadLoop::Stop()
{
	if (!m_impl)
		return;

	Cancel();
	if (m_impl->m_thread.joinable())
		m_impl->m_thread.join();
}

void ThreadLoop::Cancel()
{
	if (!m_impl)
		return;
	m_impl->m_condition = false;
}

}
