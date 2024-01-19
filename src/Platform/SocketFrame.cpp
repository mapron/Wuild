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

#include "SocketFrame.h"

#include "ByteOrderStreamTypes.h"

#include <sstream>

namespace Wuild {

SocketFrame::SocketFrame()
    : m_created(true)
{}

void SocketFrame::LogTo(std::ostream& os) const
{
    os << "T=" << m_transactionId;
    if (m_replyToTransactionId != uint64_t(-1))
        os << ", R=" << m_replyToTransactionId;
}

SocketFrame::State SocketFrame::Read(ByteOrderDataStreamReader& stream)
{
    if (m_writeLength) {
        stream >> m_length;
        if (stream.eofRead())
            return stIncomplete;

        if (!stream.getBuffer().checkRemain(m_length))
            return stIncomplete;
    }
    if (m_writeCreated)
        stream >> m_created;
    if (m_writeTransaction)
        stream >> m_transactionId >> m_replyToTransactionId;

    auto result = ReadInternal(stream);
    if (result != stOk)
        return result;

    return stOk;
}

SocketFrame::State SocketFrame::Write(ByteOrderDataStreamWriter& stream) const
{
    ptrdiff_t initialOffset = 0;
    if (m_writeLength) {
        m_length      = 0;
        initialOffset = stream.getBuffer().getOffsetWrite();
        stream << m_length;
    }
    if (m_writeCreated)
        stream << m_created;
    if (m_writeTransaction)
        stream << m_transactionId << m_replyToTransactionId;

    auto result = WriteInternal(stream);
    if (result != stOk)
        return result;

    if (m_writeLength) {
        m_length = static_cast<uint32_t>(stream.getBuffer().getOffsetWrite() - initialOffset - sizeof(m_length));
        stream.writeToOffset(m_length, initialOffset);
    }
    return result;
}

}
