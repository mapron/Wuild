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

#include "ToolProxyFrames.h"

#include <ByteOrderStream.h>

#include <utility>

namespace Wuild
{

void ToolProxyRequest::LogTo(std::ostream &os) const
{
	SocketFrame::LogTo(os);
	os << " " << m_invocation.m_id.m_toolId << " args:" << m_invocation.GetArgsString(false) << " cwd:" << m_cwd;
}

SocketFrame::State ToolProxyRequest::ReadInternal(ByteOrderDataStreamReader &stream)
{
	stream >> m_invocation.m_args;
	stream >> m_invocation.m_id.m_toolId;
	stream >> m_cwd;
	return stOk;
}

SocketFrame::State ToolProxyRequest::WriteInternal(ByteOrderDataStreamWriter &stream) const
{
	stream << m_invocation.m_args;
	stream << m_invocation.m_id.m_toolId;
	stream << m_cwd;
	return stOk;
}

ToolProxyResponse::ToolProxyResponse(std::string stdOut, bool result)
	: m_result(result)
	, m_stdOut(std::move(stdOut))
{

}

void ToolProxyResponse::LogTo(std::ostream &os) const
{
	SocketFrame::LogTo(os);
	os << " -> " << (m_result ? "OK" : "FAIL") << " [" << m_stdOut.size() << m_stdOut << "]";
}

SocketFrame::State ToolProxyResponse::ReadInternal(ByteOrderDataStreamReader &stream)
{
	stream >> m_result;
	stream >> m_stdOut;
	return stOk;
}

SocketFrame::State ToolProxyResponse::WriteInternal(ByteOrderDataStreamWriter &stream) const
{
	stream << m_result;
	stream << m_stdOut;
	return stOk;
}

}
