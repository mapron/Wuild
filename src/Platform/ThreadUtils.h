/*
 * Copyright (C) 2017-2021 Smirnov Vladimir mapron1@gmail.com
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

#include <thread>
#include <chrono>
#include <queue>
#include <mutex>

namespace Wuild {
inline void usleep(int64_t useconds)
{
    if (useconds > 0)
        std::this_thread::sleep_for(std::chrono::microseconds(useconds));
}

/// mutex-locked std::queue wrapper.
template<class T>
class ThreadSafeQueue {
    mutable std::mutex m_mutex;
    std::queue<T>      m_impl;

public:
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_impl.size();
    }
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_impl.empty();
    }

    void push(const T& val)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_impl.push(val);
    }
    bool pop(T& val)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_impl.size()) {
            val = m_impl.front();
            m_impl.pop();
            return true;
        }
        return false;
    }
};

}
