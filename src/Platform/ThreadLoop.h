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

#include "CommonTypes.h"

#include <functional>

namespace Wuild
{
class ThreadLoopImpl;
/// std::thread warpper, which will execute callback (quant) function repeatedly until stopped.
class ThreadLoop
{
	std::unique_ptr<ThreadLoopImpl> m_impl;
public:
	using QuantFunction = std::function<void(void)>;

public:
	ThreadLoop();
	ThreadLoop(ThreadLoop&& rh) noexcept;
	~ThreadLoop();

	/// thread is performing action (and was not stopped)
	bool IsRunning() const;

	/// start thread execution; thread will repeat calling quant() until stopping.
	void Exec(const QuantFunction& quant, int64_t sleepUS = 1000);

	/// Stop() interrupts thread synchronously.
	void Stop();

	/// Stop calling action callback.
	void Cancel();
};

}
