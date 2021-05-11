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

#include "RemoteToolFrames.h"

#include <ByteOrderStream.h>
#include <ByteOrderStreamTypes.h>

namespace Wuild {

void RemoteToolRequest::LogTo(std::ostream& os) const
{
    SocketFrame::LogTo(os);
    os << " " << m_invocation.m_id.m_toolId << " args:" << m_invocation.GetArgsString(false);
    os << " file: [" << m_fileData.size() << ", COMP:" << uint32_t(m_compression.m_type) << "]";
}

SocketFrame::State RemoteToolRequest::ReadInternal(ByteOrderDataStreamReader& stream)
{
    stream >> m_clientId;
    stream >> m_sessionId;
    stream >> m_fileData;
    stream >> m_invocation.m_args;
    stream >> m_invocation.m_id.m_toolId;
    stream >> m_compression;
    return stOk;
}

SocketFrame::State RemoteToolRequest::WriteInternal(ByteOrderDataStreamWriter& stream) const
{
    stream << m_clientId;
    stream << m_sessionId;
    stream << m_fileData;
    stream << m_invocation.m_args;
    stream << m_invocation.m_id.m_toolId;
    stream << m_compression;
    return stOk;
}

void RemoteToolResponse::LogTo(std::ostream& os) const
{
    SocketFrame::LogTo(os);
    os << " -> " << (m_result ? "OK" : "FAIL") << " ["
       << m_fileData.size() << ", COMP:" << uint32_t(m_compression.m_type) << "], std["
       << m_stdOut.size() << "]";
}

SocketFrame::State RemoteToolResponse::ReadInternal(ByteOrderDataStreamReader& stream)
{
    stream >> m_result;
    stream >> m_fileData;
    stream >> m_stdOut;
    stream >> m_executionTime;
    stream >> m_compression;
    return stOk;
}

SocketFrame::State RemoteToolResponse::WriteInternal(ByteOrderDataStreamWriter& stream) const
{
    stream << m_result;
    stream << m_fileData;
    stream << m_stdOut;
    stream << m_executionTime;
    stream << m_compression;
    return stOk;
}

SocketFrame::State ToolsVersionResponse::ReadInternal(ByteOrderDataStreamReader& stream)
{
    stream >> m_versions;
    return stOk;
}

SocketFrame::State ToolsVersionResponse::WriteInternal(ByteOrderDataStreamWriter& stream) const
{
    stream << m_versions;
    return stOk;
}

}
