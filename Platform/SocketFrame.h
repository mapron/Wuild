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

#include <TimePoint.h>
#include <CommonTypes.h>

#include <stdint.h>
#include <vector>
#include <memory>

namespace Wuild
{

class ByteOrderDataStreamReader;
class ByteOrderDataStreamWriter;
/**
 * \brief Abstract class for network channel frame.
 *
 * To create your own channel layer frame, subclass, create some fields and than reinmplement
 * ReadInternal and WriteInternal functions;
 * You also should override FrameTypeId to set some distinctive number for channel stream.
 */
class SocketFrame
{
public:
	enum State {
		stOk,
		stIncomplete,
		stBroken
	};
	using Ptr = std::shared_ptr<SocketFrame>;
	/// Minimal value for FrameTypeId() result.
	static const uint8_t s_minimalUserFrameId = 0x10;
public:
	/// Unique identifier for channel frame type.
	virtual uint8_t             FrameTypeId() const = 0;

	/// Output some information for logging purpose
	virtual void                LogTo(std::ostream& os) const;

	/// Deserializing frame from bytestream.
	State                       Read(ByteOrderDataStreamReader &stream);

	/// Serializing frame to bytestream.
	State                       Write(ByteOrderDataStreamWriter &stream) const;
	virtual                     ~SocketFrame() = default;

public:
	TimePoint                   m_created;                              //!< TimePoint when rame faw created
	uint64_t                    m_transactionId = 0;                    //!< Transaction  id for SocketFrameHandler
	uint64_t                    m_replyToTransactionId = uint64_t(-1);  //!< Link with some transaction request.

protected:
								SocketFrame();
								SocketFrame(const SocketFrame& another);
								SocketFrame& operator= (const SocketFrame&  another);

	virtual State               ReadInternal(ByteOrderDataStreamReader &stream) = 0;
	virtual State               WriteInternal(ByteOrderDataStreamWriter &stream) const = 0;

	mutable uint32_t            m_length = 0;
	bool                        m_writeCreated = false;                 //!< If set true in subclass, m_created will be automagically serialized.
	bool                        m_writeTransaction = false;             //!< If set true in subclass, m_transactionId/m_replyToTransactionId will be automagically serialized.
	bool                        m_writeLength = false;                  //!< If set true in subclass, then required length will be calculated, so it's no need to sheck for Incomplete frames in  ReadFromByteStreamInternal.

};

/// Convenience class to setup serialization of common fields.
class SocketFrameExt : public SocketFrame
{
public:
	SocketFrameExt() { m_writeCreated = true; m_writeTransaction = true; m_writeLength = true; }
};

}
