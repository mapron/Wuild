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
#include <SocketFrameHandler.h>
#include <ToolInvocation.h>
#include <TimePoint.h>
#include <CommonTypes.h>

/// Declaration of channel structures for RemoteToolServer and RemoteToolClient
namespace Wuild
{

class RemoteToolRequest : public SocketFrameExt
{
public:
	static const uint32_t s_version = 1;
	static const uint8_t s_frameTypeId = s_minimalUserFrameId + 1;
	using Ptr = std::shared_ptr<RemoteToolRequest>;

	std::string         m_clientId;
	uint64_t            m_sessionId;
	ToolInvocation      m_invocation;
	ByteArrayHolder     m_fileData;

	uint8_t             FrameTypeId() const override { return s_frameTypeId;}

	void                LogTo(std::ostream& os)  const override;
	State               ReadInternal(ByteOrderDataStreamReader &stream) override;
	State               WriteInternal(ByteOrderDataStreamWriter &stream) const override;

};

class RemoteToolResponse : public SocketFrameExt
{
public:
	static const uint32_t s_version = 1;
	static const uint8_t s_frameTypeId = s_minimalUserFrameId + 2;
	using Ptr = std::shared_ptr<RemoteToolResponse>;

	bool                m_result = true;
	ByteArrayHolder     m_fileData;
	std::string         m_stdOut;
	TimePoint           m_executionTime;

	void                LogTo(std::ostream& os) const override;
	uint8_t             FrameTypeId() const override { return s_frameTypeId;}

	State               ReadInternal(ByteOrderDataStreamReader &stream) override;
	State               WriteInternal(ByteOrderDataStreamWriter &stream) const override;
};

}
