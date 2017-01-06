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

#include <ctime>
#include <string>

namespace Wuild
{
/// Class describes time moment or time interval.
class TimePoint
{
    int64_t m_us; //!< microseconds

public:
    static const int64_t ONE_SECOND = 1000000LL;
    /// if now == true, then current timepoint is created.
    TimePoint(bool now = false);

    /// Creates timepoint from number of seconds
    TimePoint(int seconds);

    /// Creates timepoint from number of seconds
    TimePoint(double seconds);

    /// Checks timempoint is valid.
    inline operator bool () const { return m_us != 0; }

    /// Floored number of seconds in timepoint.
    inline int64_t GetSeconds() const { return m_us / ONE_SECOND; }

    /// Get microseconds
    inline int64_t GetUS() const { return m_us; }

    /// Set microseconds
    inline void SetUS(int64_t stamp) { m_us = stamp; }

    /// Microseconds after a whole second.
    inline int64_t GetFractionalUS() const { return m_us % ONE_SECOND; }

    /// Get time struct
    std::tm GetTm() const;

    /// Get time struct
    void SetTm(const std::tm & t, int ms = 0);

    /// Create timepoint from hms
    void SetTime(int hour, int minute, int second, int ms = 0);

    /// Convert to timepoint using system local time (for user interaction).
    TimePoint& ToLocal(); // Modify self
    TimePoint& FromLocal(); // Modify self

    /// Returns elapsed time from current value.
    inline TimePoint GetElapsedTime (const TimePoint& to = TimePoint(true)) const {
        return (to - *this);
    }

    /// Arithmetics
    inline TimePoint operator - (const TimePoint& another) const
    {
        TimePoint ret = *this;
        ret.m_us -= another.m_us;
        return ret;
    }
    inline TimePoint operator + (const TimePoint& another) const
    {
        TimePoint ret = *this;
        ret.m_us += another.m_us;
        return ret;
    }

    inline bool operator >= (const TimePoint& another) const {
        return this->m_us >= another.m_us;
    }
    inline bool operator <= (const TimePoint& another) const {
        return this->m_us <= another.m_us;
    }
    inline bool operator > (const TimePoint& another) const {
        return this->m_us > another.m_us;
    }
    inline bool operator < (const TimePoint& another) const {
        return this->m_us < another.m_us;
    }
    inline TimePoint& operator += (const TimePoint& another) {
        this->m_us += another.m_us;
        return *this;
    }
    inline TimePoint& operator -= (const TimePoint& another) {
        this->m_us -= another.m_us;
        return *this;
    }
    inline TimePoint& operator *= (int mul) {
        m_us = m_us * mul;
        return *this;
    }
    inline TimePoint& operator /= (int mul) {
        m_us = m_us / mul;
        return *this;
    }

    /// Printable format, like [YYYY-MM-DD] hh:mm:ss[.zzz]
    std::string ToString(bool printMS = true, bool printDate = false) const;

private:
    static int LocalOffsetSeconds();
};
}
