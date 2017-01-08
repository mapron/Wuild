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

#include "CoordinatorFrames.h"

#include <ByteOrderStream.h>

namespace Wuild
{

template<>
inline ByteOrderDataStreamReader& ByteOrderDataStreamReader::operator >> (TimePoint &point)
{
	point.SetUS( this->ReadScalar<int64_t>() );
	return *this;
}
template<>
inline ByteOrderDataStreamWriter& ByteOrderDataStreamWriter::operator << (const TimePoint & point)
{
	*this << point.GetUS();
	return *this;
}

template<>
inline ByteOrderDataStreamReader& ByteOrderDataStreamReader::operator >> (ToolServerInfo::ConnectedClientInfo &client)
{
	*this
		>> client.m_usedThreads
		>> client.m_clientHost
		>> client.m_clientId
		>> client.m_sessionId
	   ;
	return *this;
}
template<>
inline ByteOrderDataStreamWriter& ByteOrderDataStreamWriter::operator << (const ToolServerInfo::ConnectedClientInfo &client)
{
	*this
		<< client.m_usedThreads
		<< client.m_clientHost
		<< client.m_clientId
		<< client.m_sessionId
			;
	return *this;
}

template<>
inline ByteOrderDataStreamReader& ByteOrderDataStreamReader::operator >> (ToolServerInfo &info)
{
	*this
		>> info.m_toolServerId
		>> info.m_connectionHost
		>> info.m_connectionPort
		>> info.m_toolIds
		>> info.m_totalThreads
		>> info.m_connectedClients
			;
	return *this;
}
template<>
inline ByteOrderDataStreamWriter& ByteOrderDataStreamWriter::operator << (const ToolServerInfo &info)
{
	*this
		<< info.m_toolServerId
		<< info.m_connectionHost
		<< info.m_connectionPort
		<< info.m_toolIds
		<< info.m_totalThreads
		<< info.m_connectedClients
	   ;
	return *this;
}
template<>
inline ByteOrderDataStreamReader& ByteOrderDataStreamReader::operator >> (ToolServerSessionInfo &session)
{
	*this
		>> session.m_clientId
		>> session.m_sessionId
		>> session.m_totalExecutionTime
		>> session.m_tasksCount
		>> session.m_failuresCount
		   ;
	return *this;
}
template<>
inline ByteOrderDataStreamWriter& ByteOrderDataStreamWriter::operator << (const ToolServerSessionInfo &session)
{
	*this
		<< session.m_clientId
		<< session.m_sessionId
		<< session.m_totalExecutionTime
		<< session.m_tasksCount
		<< session.m_failuresCount
		   ;
	return *this;
}


SocketFrame::State CoordinatorListResponse::ReadInternal(ByteOrderDataStreamReader &stream)
{
	stream  >> m_info.m_toolServers >> m_latestSessions;

	return stOk;
}

SocketFrame::State CoordinatorListResponse::WriteInternal(ByteOrderDataStreamWriter &stream) const
{
	stream << m_info.m_toolServers <<  m_latestSessions;
	return stOk;
}

SocketFrame::State CoordinatorToolServerStatus::ReadInternal(ByteOrderDataStreamReader &stream)
{
	stream >> m_info;
	return stOk;
}

SocketFrame::State CoordinatorToolServerStatus::WriteInternal(ByteOrderDataStreamWriter &stream) const
{
	stream << m_info;
	return stOk;
}

SocketFrame::State CoordinatorToolServerSession::ReadInternal(ByteOrderDataStreamReader &stream)
{
	stream >> m_session;
	return stOk;
}

SocketFrame::State CoordinatorToolServerSession::WriteInternal(ByteOrderDataStreamWriter &stream) const
{
	stream << m_session;
	return stOk;
}


}
