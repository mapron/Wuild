/*
 * Copyright (C) 2018-2021 Smirnov Vladimir mapron1@gmail.com
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

#include "MernelPlatform/ByteOrderStream.hpp"
#include "MernelPlatform/Compression.hpp"

#include "TimePoint.h"
#include "CommonTypes.h"

namespace Wuild {
using ByteOrderDataStreamReader = Mernel::ByteOrderDataStreamReader;
using ByteOrderDataStreamWriter = Mernel::ByteOrderDataStreamWriter;
using ByteOrderBuffer           = Mernel::ByteOrderBuffer;

}

namespace Mernel {

template<>
inline ByteOrderDataStreamReader& ByteOrderDataStreamReader::operator>>(Wuild::TimePoint& point)
{
    point.SetUS(this->readScalar<int64_t>());
    return *this;
}
template<>
inline ByteOrderDataStreamWriter& ByteOrderDataStreamWriter::operator<<(const Wuild::TimePoint& point)
{
    *this << point.GetUS();
    return *this;
}

template<>
inline ByteOrderDataStreamReader& ByteOrderDataStreamReader::operator>>(ByteArrayHolder& point)
{
    point.resize(this->readScalar<uint32_t>());
    if (point.size())
        this->readBlock(point.data(), point.size());
    return *this;
}

template<>
inline ByteOrderDataStreamWriter& ByteOrderDataStreamWriter::operator<<(const ByteArrayHolder& point)
{
    uint32_t filesize = point.size();
    *this << filesize;
    if (filesize)
        this->writeBlock(point.data(), point.size());
    return *this;
}

template<>
inline ByteOrderDataStreamReader& ByteOrderDataStreamReader::operator>>(Mernel::CompressionInfo& info)
{
    uint32_t level = 0, compType = 0;
    *this >> compType >> level;
    info.m_type  = static_cast<Mernel::CompressionType>(compType);
    info.m_level = static_cast<int>(level);
    return *this;
}

template<>
inline ByteOrderDataStreamWriter& ByteOrderDataStreamWriter::operator<<(const Mernel::CompressionInfo& info)
{
    *this << static_cast<uint32_t>(info.m_type);
    *this << static_cast<uint32_t>(info.m_level);
    return *this;
}

}
