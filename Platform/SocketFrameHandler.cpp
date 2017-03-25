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

#include "SocketFrameHandler.h"

#include <TcpSocket.h>
#include <ThreadUtils.h>
#include <Application.h>
#include <ByteOrderStream.h>

#include <cstring>
#include <stdexcept>
#include <sstream>
#include <assert.h>

#define BUFFER_RATIO 8 / 10

namespace Wuild
{
SocketFrameHandlerSettings::SocketFrameHandlerSettings()
	: m_channelActivityTimeout(false)
	, m_byteOrder( ByteOrderDataStream::CreateByteorderMask(ORDER_BE, ORDER_BE, ORDER_BE))
{}

SocketFrameHandler::SocketFrameHandler(const SocketFrameHandlerSettings & settings)
	: m_settings(settings)
	, m_acknowledgeTimer(true)
{
	m_maxUnAcknowledgedSize = 4 * 1024 * BUFFER_RATIO;// 4 Kb is a minimal socket buffer.
	if (m_settings.m_hasConnOptions)
		m_setConnectionOptionsNeedSend = true;
	m_aliveHolder.reset(new AliveStateHolder());
}

SocketFrameHandler::~SocketFrameHandler()
{
	m_aliveHolder->m_isAlive = false;
	Stop();
}

// Process cycle management:

void SocketFrameHandler::SetRetryConnectOnFail(bool retry)
{
	m_retryConnectOnFail = retry;
}

void SocketFrameHandler::Start()
{
	m_thread.Exec([this]{
		if (!this->Quant())
		{
			this->DisconnectChannel();
			m_thread.Stop(false);
		}
	}, m_settings.m_clientThreadSleep.GetUS());
}

void SocketFrameHandler::Stop(bool wait)
{
	m_thread.Stop(wait);
}

bool SocketFrameHandler::Quant()
{
	// Each quant, we check connection, if we ok, then read and write frames data.
	// If false returned, thread will interrupted.
	ConnectionState connectionState = this->CheckAndCreateConnection() ? ConnectionState::Ok : ConnectionState::Failed;
	SetConnectionState(connectionState);
	if (connectionState != ConnectionState::Ok)
		return m_retryConnectOnFail;

	if (m_channel && m_channel->IsPending())
		return true;

	bool taskResult = ReadFrames() && WriteFrames();
	if (!taskResult)
	{
		Syslogger(m_logContext, Syslogger::Info) << "read/write frames return false, " << ( m_retryConnectOnFail ? "waiting." : "stopping.");
		DisconnectChannel();
		SetConnectionState(ConnectionState::Failed);
		if (m_retryConnectOnFail)
			usleep(m_settings.m_afterDisconnectWait.GetUS());
		return m_retryConnectOnFail;
	}
	if (m_lastTimeoutCheck.GetElapsedTime() > m_settings.m_replyTimeoutCheckInterval)
	{
		m_lastTimeoutCheck = TimePoint(true);
		m_replyManager.CheckTimeouts();
	}
	return true;
}

bool SocketFrameHandler::IsActive() const
{
	return m_thread.IsRunning() && this->CheckConnection();
}

// Underlying channel functions:

void SocketFrameHandler::SetTcpChannel(const std::string& host, int port, TimePoint connectionTimeout)
{
	TcpConnectionParams params;
	params.SetPoint(port, host);
	params.m_connectTimeout = connectionTimeout;
	params.m_readTimeout = m_settings.m_tcpReadTimeout;
	params.m_recommendedRecieveBufferSize = m_settings.m_recommendedRecieveBufferSize;
	params.m_recommendedSendBufferSize    = m_settings.m_recommendedSendBufferSize;
	TcpSocket::Create(params).swap(m_channel);
	UpdateLogContext();
}

void SocketFrameHandler::SetChannel(IDataSocket::Ptr channel)
{
	m_channel = channel;
	UpdateLogContext();
}

void SocketFrameHandler::DisconnectChannel()
{
	if (m_channel)
		m_channel->Disconnect();
}

void SocketFrameHandler::SetChannelNotifier(StateNotifierCallback stateNotifier)
{
	m_stateNotifier = stateNotifier;
}
// Application logic:

void SocketFrameHandler::QueueFrame(SocketFrame::Ptr message, SocketFrameHandler::ReplyNotifier replyNotifier, TimePoint timeout)
{
	if (replyNotifier)
	{
		message->m_transactionId = m_transaction++;
		m_replyManager.AddNotifier(message->m_transactionId, replyNotifier, timeout);
	}

	m_framesQueueOutput.push(message);
}

void SocketFrameHandler::RegisterFrameReader(SocketFrameHandler::IFrameReader::Ptr reader)
{
	if (reader->FrameTypeId() < SocketFrame::s_minimalUserFrameId)
		throw std::logic_error("Userframe ids should should start from " + std::to_string(SocketFrame::s_minimalUserFrameId));

   if (m_frameReaders.find( reader->FrameTypeId() ) != m_frameReaders.end())
		throw std::logic_error("Frame reader already exists.");

   m_frameReaders[reader->FrameTypeId()] = reader;
}

void SocketFrameHandler::SetLogContext(const std::string &context)
{
	m_logContextAdditional = context;
}

void SocketFrameHandler::UpdateLogContext()
{
	std::string channelContext;
	if (m_channel)
	{
		channelContext = m_channel->GetLogContext();
		if (!m_logContextAdditional.empty())
			channelContext += " ";
	}
	m_logContext = channelContext + m_logContextAdditional;
}

// protected:

void SocketFrameHandler::SetConnectionState(SocketFrameHandler::ConnectionState connectionState)
{
	if (connectionState == m_prevConnectionState)
		return;

	if (m_stateNotifier)
		m_stateNotifier(connectionState == ConnectionState::Ok);
	if (m_prevConnectionState != ConnectionState::Pending && connectionState == ConnectionState::Failed)
	{
		m_replyManager.ClearAndSendError();
	}
	if (connectionState == ConnectionState::Ok)
	{
		m_lastSucceessfulRead = TimePoint(true);
	}
	m_prevConnectionState = connectionState;
	UpdateLogContext();
}

bool SocketFrameHandler::ReadFrames()
{
	// first, try to read some data from socket
	const size_t currentSize = m_readBuffer.GetHolder().size();
	const auto readState = m_channel->Read(m_readBuffer.GetHolder());

	if (readState == IDataSocket::ReadState::Fail)
		return false;

	if (readState == IDataSocket::ReadState::TryAgain)
		return true;//nothing to read, it's not a error.

	m_lastTestActivity = m_lastSucceessfulRead = TimePoint(true);

	const size_t newSize = m_readBuffer.GetHolder().size();

	m_readBuffer.SetSize(newSize);

	m_doTestActivity = true;
	m_readBuffer.ResetRead();

	m_outputAcknowledgesSize += newSize - currentSize;
	bool validInput = true;

	// if some new data arrived, try to extract segments from it:
	do
	{
		ConsumeState state = ConsumeReadBuffer();
		if (state == ConsumeState::FatalError)
			return false;
		else if (state == ConsumeState::Broken)
			validInput = false;

		if (state != ConsumeState::Ok || m_readBuffer.EofRead())
			break;

		m_readBuffer.RemoveFromStart(m_readBuffer.GetOffsetRead());

	} while (m_readBuffer.GetSize());

	m_frameDataBuffer.ResetRead();

	// if we have read frame data, try to parse it (and process apllication frames):
	do
	{
		if (!validInput || m_pendingReadType == ServiceMessageType::None)
			break;

		ConsumeState state = ConsumeFrameBuffer();
		if (state == ConsumeState::FatalError)
			return false;
		else if (state == ConsumeState::Broken)
			validInput = false;

		if (state != ConsumeState::Ok || m_frameDataBuffer.EofRead())
			break;

		m_frameDataBuffer.RemoveFromStart(m_frameDataBuffer.GetOffsetRead());
	} while (m_frameDataBuffer.GetSize());

	if (!m_frameDataBuffer.GetSize())
	{
		m_pendingReadType = ServiceMessageType::None;
	}

	// if error occured, clean all buffers.
	if (!validInput)
	{
		m_readBuffer.Clear();
		m_frameDataBuffer.Clear();
		m_pendingReadType = ServiceMessageType::None;
	}

	return true;
}

SocketFrameHandler::ConsumeState SocketFrameHandler::ConsumeReadBuffer()
{
	// create stream for reading
	ByteOrderDataStreamReader inputStream(&m_readBuffer, m_settings.m_byteOrder);
	ServiceMessageType mtype = ServiceMessageType::User;
	if (m_settings.m_hasChannelTypes)
		mtype = SocketFrameHandler::ServiceMessageType(inputStream.ReadScalar<uint8_t>());

	if (m_readBuffer.GetRemainRead() < 0)
		return ConsumeState::Incomplete;

	// check for some service types: acknowledge, line test and connection options:
	if (m_settings.m_hasAcknowledges && mtype == ServiceMessageType::Ack)
	{
		uint32_t size = 0;
		inputStream >> size;
		m_acknowledgeTimer = TimePoint(true);
		m_bytesWaitingAcknowledge -= std::min(m_bytesWaitingAcknowledge, static_cast<size_t>(size));
	}
	else if (m_settings.m_hasLineTest    && mtype == ServiceMessageType::LineTest) { } // do nothing
	else if (m_settings.m_hasConnOptions && mtype == ServiceMessageType::ConnOptions)
	{
		uint32_t bufferSize, version;
		int64_t timestamp;
		inputStream >> bufferSize >> version >> timestamp;
		TimePoint remoteTime;
		remoteTime.SetUS(timestamp);
		TimePoint now(true);

		m_remoteTimeDiffToPast = now - remoteTime;

		auto tcpch = std::dynamic_pointer_cast<TcpSocket>(m_channel);
		const auto sendSize = tcpch ? tcpch->GetSendBufferSize() : 0;

		m_maxUnAcknowledgedSize = std::min(sendSize,  bufferSize) * BUFFER_RATIO;
		if (version != m_settings.m_channelProtocolVersion)
		{
			Syslogger(m_logContext, Syslogger::Err) << "Remote version is  " << version << ", but mine is " << m_settings.m_channelProtocolVersion;
			return ConsumeState::FatalError;
		}
		Syslogger(m_logContext)<< "Recieved buffer size = " << bufferSize << ", MaxUnAck=" << m_maxUnAcknowledgedSize  <<  ", remote time is " << m_remoteTimeDiffToPast.ToString() << " in past compare to me. (" << m_remoteTimeDiffToPast.GetUS() << " us)";
	}
	// othrewise, we have application frame. Move its data to framebuffer.
	else
	{
		if (m_pendingReadType != ServiceMessageType::None && m_pendingReadType != mtype)
			return ConsumeState::Incomplete; // returning incomplete when pending type changes to force ConsumeFrameBuffer call.

		if (m_frameReaders.find(int(mtype)) == m_frameReaders.end())
		{
			Syslogger(m_logContext, Syslogger::Err) << "MessageHandler: invalid type of SocketFrame = " << int(mtype) ;
			return ConsumeState::Broken;
		}

		m_pendingReadType = mtype;

		ptrdiff_t frameLength = m_readBuffer.GetRemainRead();
		if (frameLength <= 0)
			return ConsumeState::Incomplete;
		if (m_settings.m_hasChannelTypes)
		{
			uint32_t size = 0;
			inputStream >> size;
			if (size > m_settings.m_segmentSize)
			{

				Syslogger(m_logContext, Syslogger::Err) << "Invalid segment size =" << size;
				return ConsumeState::Broken;
			}
			if (size > frameLength)
				return ConsumeState::Incomplete; // incomplete read buffer;

			frameLength = size;
		}

		// we could overread at this point, check this.
		auto * framePos = m_frameDataBuffer.PosWrite(frameLength);
		assert(framePos);
		if (inputStream.ReadBlock(framePos, frameLength))
			m_frameDataBuffer.MarkWrite(frameLength);
	}

	return ConsumeState::Ok;
}

SocketFrameHandler::ConsumeState SocketFrameHandler::ConsumeFrameBuffer()
{
	// determine application frame type and create appropriate reader for it.
	uint8_t mtypei = static_cast<uint8_t>(m_pendingReadType);
	SocketFrame::Ptr incoming(m_frameReaders[mtypei]->FrameFactory());
	SocketFrame::State framestate;
	try
	{
		ByteOrderDataStreamReader frameStream(&m_frameDataBuffer, m_settings.m_byteOrder);
		framestate = incoming->Read(frameStream);
	}
	catch(std::exception & ex)
	{
		Syslogger(m_logContext, Syslogger::Err) << "MessageHandler ConsumeFrameBuffer() exception: " << ex.what();
		return ConsumeState::Broken;
	}
	if (framestate == SocketFrame::stIncomplete || m_frameDataBuffer.EofRead())
	{
		return ConsumeState::Incomplete;
	}
	else if (framestate == SocketFrame::stBroken)
	{
		Syslogger(m_logContext, Syslogger::Err) << "MessageHandler: broken message recieved. ";
		return ConsumeState::Broken;
	}
	else if (framestate == SocketFrame::stOk)
	{
		// handle read frame
		PreprocessFrame(incoming);
		return ConsumeState::Ok;
	}
	return ConsumeState::Broken;
}

bool SocketFrameHandler::WriteFrames()
{
	// check temeouted ACKs (TODO: move code?)
	if (m_settings.m_hasAcknowledges && m_bytesWaitingAcknowledge >= m_maxUnAcknowledgedSize)
	{
		if (m_acknowledgeTimer.GetElapsedTime() > m_settings.m_acknowledgeTimeout)
		{
			Syslogger(m_logContext, Syslogger::Err) << "Acknowledge not recieved!"
											<< " acknowledgeTimer=" << m_acknowledgeTimer.ToString()
											<< " now:" << TimePoint(true).ToString()
											<< " acknowledgeTimeout=" << m_settings.m_acknowledgeTimeout.ToString()
											   ;
			m_bytesWaitingAcknowledge = 0; // terminate thread - it's configuration error.
			return false;
		}
		return true;
	}
	// write ack if needed
	if (m_settings.m_hasAcknowledges && m_outputAcknowledgesSize > m_settings.m_acknowledgeMinimalReadSize)
	{
		ByteOrderDataStreamWriter streamWriter(m_settings.m_byteOrder);
		streamWriter << uint8_t(ServiceMessageType::Ack);
		streamWriter << static_cast<uint32_t>(m_outputAcknowledgesSize);
		m_outputAcknowledgesSize = 0;
		m_outputSegments.push_front(streamWriter.GetBuffer().GetHolder()); // acknowledges has high priority, so pushing them to front!
	}

	// check and write line test byte
	if (IsOutputBufferEmpty()
		&& m_settings.m_hasLineTest
		&& m_lastTestActivity.GetElapsedTime() > m_settings.m_lineTestInterval
		&& m_framesQueueOutput.empty())
	{
		ByteOrderDataStreamWriter streamWriter(m_settings.m_byteOrder);
		streamWriter << uint8_t(ServiceMessageType::LineTest);
		m_outputSegments.push_back(streamWriter.GetBuffer().GetHolder());
	}

	// send connection options
	if (m_setConnectionOptionsNeedSend)
	{
		m_setConnectionOptionsNeedSend = false;
		auto tcpch = std::dynamic_pointer_cast<TcpSocket>(m_channel);
		uint32_t size = tcpch ? tcpch->GetRecieveBufferSize() : 0;
		ByteOrderDataStreamWriter streamWriter(m_settings.m_byteOrder);
		streamWriter << uint8_t(ServiceMessageType::ConnOptions);
		streamWriter << size << m_settings.m_channelProtocolVersion << TimePoint(true).GetUS();
		m_outputSegments.push_back(streamWriter.GetBuffer().GetHolder());
	}

	// get all outpgoing frames and serialize them into channel segments
	while (!m_framesQueueOutput.empty())
	{
		SocketFrame::Ptr frontMsg;
		if (!m_framesQueueOutput.pop(frontMsg))
			throw std::logic_error("Invalid queue logic");

		ByteOrderDataStreamWriter streamWriter(m_settings.m_byteOrder);
		Syslogger(m_logContext, Syslogger::Info) << "outgoung -> " << frontMsg;
		frontMsg->Write(streamWriter);

		const auto typeId = frontMsg->FrameTypeId();
		const ByteArrayHolder & buffer = streamWriter.GetBuffer().GetHolder();

		//Syslogger(m_logContext, Syslogger::Info) << "buffer -> " << streamWriter.GetBuffer().ToHex();

		/// splitting onto segments.
		for (size_t offset = 0; offset < buffer.size(); offset += m_settings.m_segmentSize)
		{
			ByteArrayHolder bufferPart;
			const size_t length = std::min(m_settings.m_segmentSize, buffer.size() - offset);
			if (m_settings.m_hasChannelTypes)
			{
				ByteOrderBuffer partStreamBuf(bufferPart);
				ByteOrderDataStreamWriter partStreamWriter(&partStreamBuf, m_settings.m_byteOrder);
				partStreamWriter << typeId << uint32_t(length);
			}
			size_t curSize = bufferPart.ref().size();
			bufferPart.ref().resize(curSize + length);
			memcpy(bufferPart.data() + curSize, buffer.data() + offset , length);
			m_outputSegments.push_back(bufferPart);
		}
	}

	// write all outgoing segments to tcp socket
	while (!m_outputSegments.empty())
	{
		ByteArrayHolder frontSegment = m_outputSegments.front();

		const auto sizeForWrite = frontSegment.size();
		const auto maxSize = m_maxUnAcknowledgedSize - m_bytesWaitingAcknowledge;
		if (m_settings.m_hasAcknowledges && sizeForWrite > maxSize && sizeForWrite > 1) // allow writeing of test frame.
			break;

		auto writeResult = m_channel->Write(frontSegment, sizeForWrite);
		if (writeResult == IDataSocket::WriteState::TryAgain)
			break;

		m_outputSegments.pop_front();
		if (writeResult == IDataSocket::WriteState::Fail)
		{
			Syslogger(m_logContext, m_settings.m_writeFailureLogLevel) << "Write failed: sizeForWrite=" <<  sizeForWrite
											 << ", maxSize=" << maxSize
											 << ", m_bytesWaitingAcknowledge=" << m_bytesWaitingAcknowledge
												;
			return false;
		}

		m_lastTestActivity = TimePoint(true);
		if (m_settings.m_hasAcknowledges)
		{
			m_acknowledgeTimer = m_lastTestActivity;
			m_bytesWaitingAcknowledge += sizeForWrite;
			if (m_bytesWaitingAcknowledge >= m_maxUnAcknowledgedSize)
				break;
		}
	}
	return true;
}

bool SocketFrameHandler::CheckConnection() const
{
	bool result = m_channel && (m_channel->IsConnected() || m_channel->IsPending());

	bool wasActivity = m_settings.m_channelActivityTimeout <= TimePoint(0) || m_lastSucceessfulRead.GetElapsedTime() < m_settings.m_channelActivityTimeout;
	if (result && m_doTestActivity && !wasActivity)
	{
		m_channel->Disconnect();
		result = false;
	}
	if (!result)
	{
		m_doTestActivity = false;
		if (m_settings.m_hasConnOptions)
			m_setConnectionOptionsNeedSend = true;
	}

	return result;
}

bool SocketFrameHandler::CheckAndCreateConnection()
{
	if (!m_channel)
		return false;

	if (!m_channel->IsConnected())
		m_channel->Connect();
	return CheckConnection();
}

bool SocketFrameHandler::IsOutputBufferEmpty()
{
	return m_outputSegments.empty();
}

void SocketFrameHandler::PreprocessFrame(SocketFrame::Ptr incomingMessage)
{
	// if frame has reply callback, use it. Otherwise, call ProcessFrame on frameReader.
	Syslogger(m_logContext, Syslogger::Info) << "incoming <- " << incomingMessage;
	auto notifyCallback = m_replyManager.TakeNotifier(incomingMessage->m_replyToTransactionId);
	if (notifyCallback)
	{
		notifyCallback(incomingMessage, ReplyState::Success);
	}
	else
	{
		auto tid = incomingMessage->m_transactionId;
		// frame factory succeeded, so it's safe do not check TypeId.
		m_frameReaders[incomingMessage->FrameTypeId()]->ProcessFrame(incomingMessage, [handler=this, aliveHolder=m_aliveHolder, tid](SocketFrame::Ptr reply)
		{
			if (!aliveHolder->m_isAlive)
			{
				Syslogger() << "SocketFrameHandler is dead! skipping outgouing frame.";
				return;
			}
			reply->m_replyToTransactionId = tid;
			handler->QueueFrame(reply);
		});
	}
}

void SocketFrameHandler::ReplyManager::ClearAndSendError()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	for (auto &p : m_replyNotifiers)
		p.second(nullptr, ReplyState::Error);

	m_replyNotifiers.clear();
	m_timeouts.clear();
}

void SocketFrameHandler::ReplyManager::AddNotifier(uint64_t id, SocketFrameHandler::ReplyNotifier callback, TimePoint timeout)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_replyNotifiers[id] = callback;
	if (timeout)
	{
		TimePoint expiration = TimePoint(true) + timeout;
		m_timeouts.emplace(id, expiration);
	}
}

void SocketFrameHandler::ReplyManager::CheckTimeouts()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	TimePoint now(true);
	auto timeoutsIt = m_timeouts.begin();
	while (timeoutsIt != m_timeouts.end())
	{
		uint64_t id = timeoutsIt->first;
		TimePoint deadline = timeoutsIt->second;
		if (deadline < now)
		{
			auto search = m_replyNotifiers.find(id);
			if (search != m_replyNotifiers.end())
			{
				search->second(nullptr, ReplyState::Timeout);
				m_replyNotifiers.erase(search);
			}
			else
			{
				Syslogger(Syslogger::Crit) << "Timeout request =" << id << "does not has callback.";
			}

			timeoutsIt = m_timeouts.erase(timeoutsIt);
			continue;
		}
		++timeoutsIt;
	}
}

SocketFrameHandler::ReplyNotifier SocketFrameHandler::ReplyManager::TakeNotifier(uint64_t id)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto timeoutsIt = m_timeouts.find(id);
	if (timeoutsIt != m_timeouts.end())
		m_timeouts.erase(timeoutsIt);

	auto replyIt = m_replyNotifiers.find(id);
	if (replyIt != m_replyNotifiers.end())
	{
		auto callback = replyIt->second;
		m_replyNotifiers.erase(replyIt);
		return callback;
	}

	return ReplyNotifier();
}

}
