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

#include "RemoteToolClientConfig.h"

#include <iostream>
#include <algorithm>

namespace Wuild {

bool RemoteToolClientConfig::Validate(std::ostream* errStream) const
{
    if (m_queueTimeout <= TimePoint(0)) {
        if (errStream)
            *errStream << "queueTimeout should be greater than 0.";
        return false;
    }
    if (m_invocationAttempts <= 0) {
        if (errStream)
            *errStream << "invocationAttempts should be at least 1.";
        return false;
    }
    return m_coordinator.Validate(errStream);
}

void RemoteToolClientConfig::PostProcess::Apply(ByteArray& data) const
{
    auto replaceOnce = [&data](const ByteArray& needle, const ByteArray& replacement) -> bool {
        auto it = std::search(data.begin(), data.end(), needle.begin(), needle.end());
        if (it == data.end())
            return false;
        std::copy(replacement.cbegin(), replacement.cend(), it);
        return true;
    };
    for (auto&& item : m_items) {
        const auto& needle      = item.m_needle;
        const auto& replacement = item.m_replacement;
        while (replaceOnce(needle, replacement)) {
        }
    }
}

}
