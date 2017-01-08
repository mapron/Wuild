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
}

SocketFrameHandler::~SocketFrameHandler()
{
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
	bool check = this->CheckAndCreateConnection();
	if (check != m_prevConnectionState || !m_initChannel)
	{
		m_initChannel = true;
		m_prevConnectionState = check;
		if (m_stateNotifier)
			m_stateNotifier(check);
		if (!check)
		{
			for (auto &p : m_replyNotifiers)
				p.second(nullptr, rsError);

			m_replyNotifiers.clear();
		}
		else
		{
			m_lastSucceessfulRead = TimePoint(true);
		}
	}
	if (!check)
		return m_retryConnectOnFail;

	if (m_channel && m_channel->IsPending())
		return true;

	bool taskResult = ReadFrames() && WriteFrames();
	if (!taskResult)
	{
		Syslogger(m_logContext, LOG_INFO) << "read/write frames return false, " << ( m_retryConnectOnFail ? "waiting." : "stopping.");
		DisconnectChannel();
		if (m_retryConnectOnFail)
			usleep(m_settings.m_afterDisconnectWait.GetUS());
		return m_retryConnectOnFail;
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
	if (!params.SetPoint(port, host))
		return;

	params.m_connectTimeout = connectionTimeout;
	params.m_readTimeout = m_settings.m_tcpReadTimeout;
	params.m_recommendedRecieveBufferSize = m_settings.m_recommendedRecieveBufferSize;
	TcpSocket::Create(params).swap(m_channel);
}

void SocketFrameHandler::SetChannel(IDataSocket::Ptr channel)
{
	m_channel = channel;
}

void SocketFrameHandler::DisconnectChannel()
{
	if (m_channel)
		m_channel->Disconnect();
}

void SocketFrameHandler::SetChannelNotifier(TStateNotifier stateNotifier)
{
	m_stateNotifier = stateNotifier;
}
// Application logic:

void SocketFrameHandler::QueueFrame(SocketFrame::Ptr message, SocketFrameHandler::ReplyNotifier replyNotifier)
{
	if (replyNotifier)
	{
		message->m_transactionId = m_transaction++;
		m_replyNotifiers[message->m_transactionId] = replyNotifier;
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
	m_logContext = context;
}

// protected:

bool SocketFrameHandler::ReadFrames()
{
	const size_t currentSize = m_readBuffer.GetHolder().size();
	if (!m_channel->Read(m_readBuffer.GetHolder()))
		return true; //nothing to read, it's not a error. Broken channel handled separately.

	m_lastTestActivity = m_lastSucceessfulRead = TimePoint(true);

	const size_t newSize = m_readBuffer.GetHolder().size();
	m_readBuffer.SetSize(newSize);
	ByteOrderDataStreamReader inputStream(&m_readBuffer, m_settings.m_byteOrder);

	m_doTestActivity = true;
	m_readBuffer.ResetRead();

	//auto readSize = m_readBuffer.GetSize();
	//if (readSize > 50) readSize = 50;
	//Syslogger(m_logContext) << "m_readBuffer=" << Syslogger::Binary(m_readBuffer.begin(), readSize);
	//Syslogger(m_logContext) << "m_readBuffer=" << m_readBuffer.ToHex(true, true);

	//Syslogger(m_logContext) << "m_frameDataBuffer=" << m_frameDataBuffer.ToHex(true, true);

	m_outputAcknowledgesSize += newSize - currentSize;
	bool validInput = true;
	do
	{
		ServiceMessageType mtype = ServiceMessageType::User;

		if (m_settings.m_hasChannelTypes)
		{
			mtype = SocketFrameHandler::ServiceMessageType(inputStream.ReadScalar<uint8_t>());
		}

		SocketFrame::State state;
		if (m_settings.m_hasAcknowledges && mtype == ServiceMessageType::Ack)
		{
			uint32_t size = 0;
			inputStream >> size;
			m_acknowledgeTimer = TimePoint(true);
			m_bytesWaitingAcknowledge -= std::min(m_bytesWaitingAcknowledge, static_cast<size_t>(size));
		}
		else if (m_settings.m_hasLineTest && mtype == ServiceMessageType::LineTest)
		{
			// do nothing
		}
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
				Syslogger(m_logContext, LOG_ERR) << "Remote version is  " << version << ", but mine is " << m_settings.m_channelProtocolVersion;
				return false;
			}
			Syslogger(m_logContext)<< "Recieved buffer size = " << bufferSize << ", MaxUnAck=" << m_maxUnAcknowledgedSize  <<  ", remote time is " << m_remoteTimeDiffToPast.ToString() << " in past compare to me. (" << m_remoteTimeDiffToPast.GetUS() << " us)";
		}
		else
		{
			size_t frameLength = m_readBuffer.GetRemainRead();
			if (!frameLength) break;
			if (m_settings.m_hasChannelTypes)
			{
				uint32_t size = 0;
				inputStream >> size;
				if (size > m_settings.m_segmentSize)
				{
					validInput = false;
					Syslogger(m_logContext, LOG_ERR) << "Invalid segment size =" << size;
					break;
				}
				if (size > frameLength)
					break; // incomplete read buffer;

				frameLength = size;
			}
			m_frameDataBuffer.ResetRead();
			auto * framePos = m_frameDataBuffer.PosWrite(frameLength);
			assert(framePos);
			inputStream.ReadBlock(framePos, frameLength);
			m_readBuffer.RemoveFromStart(m_readBuffer.GetOffsetRead());
			m_frameDataBuffer.MarkWrite(frameLength);

			//auto fdSize = m_frameDataBuffer.GetSize();
			//if (fdSize > 50) fdSize = 50;
			//Syslogger(m_logContext) << "m_frameDataBuffer=" << Syslogger::Binary(m_frameDataBuffer.begin(), readSize);

			//Syslogger(m_logContext) << "m_frameDataBuffer=" << m_frameDataBuffer.ToHex(true, true);
			//Syslogger(m_logContext) << "m_readBuffer=" << m_readBuffer.ToHex(true, true);

			uint8_t mtypei = static_cast<uint8_t>(mtype);
			if (m_frameReaders.find(mtypei) == m_frameReaders.end())
			{
				validInput = false;
				Syslogger(m_logContext, LOG_ERR) << "MessageHandler: invalid type of SocketFrame. " << int(mtype) << " m_okFrames=" << m_okFrames;
				break;
			}
			SocketFrame::Ptr incoming(m_frameReaders[mtypei]->FrameFactory());
			try
			{
				ByteOrderDataStreamReader frameStream(&m_frameDataBuffer, m_settings.m_byteOrder);
				state = incoming->Read(frameStream);
			}
			catch(std::exception & ex)
			{
				validInput = false;
				Syslogger(m_logContext, LOG_ERR) << "MessageHandler Read() exception: " << ex.what();
				break;
			}
			if (state == SocketFrame::stIncomplete || m_frameDataBuffer.EofRead())
			{
				m_frameDataBuffer.ResetRead();
				continue;
			}
			else if (state == SocketFrame::stBroken)
			{
				validInput = false;
				Syslogger(m_logContext, LOG_ERR) << "MessageHandler: broken message recieved. ";
				break;
			}
			else if (state == SocketFrame::stOk)
			{
				m_okFrames++;
				PreprocessFrame(incoming);
			}
		}


		m_readBuffer.RemoveFromStart(m_readBuffer.GetOffsetRead());
		m_frameDataBuffer.RemoveFromStart(m_frameDataBuffer.GetOffsetRead());
	} while (m_readBuffer.GetSize());

	if (!validInput)
	{
		// removing all read data.
		m_readBuffer.Clear();
		m_frameDataBuffer.Clear();
	}
	return true;
}

bool SocketFrameHandler::WriteFrames()
{
	if (m_settings.m_hasAcknowledges && m_bytesWaitingAcknowledge >= m_maxUnAcknowledgedSize)
	{
		if (m_acknowledgeTimer.GetElapsedTime() > m_settings.m_acknowledgeTimeout)
		{
			Syslogger(m_logContext, LOG_ERR) << "Acknowledge not recieved!"
											<< " acknowledgeTimer=" << m_acknowledgeTimer.ToString()
											<< " now:" << TimePoint(true).ToString()
											<< " acknowledgeTimeout=" << m_settings.m_acknowledgeTimeout.ToString()
											   ;
			m_bytesWaitingAcknowledge = 0; // terminate thread - it's configuration error.
			return false;
		}
		return true;
	}

	if (m_settings.m_hasAcknowledges && m_outputAcknowledgesSize > m_settings.m_acknowledgeMinimalReadSize)
	{
		ByteOrderDataStreamWriter streamWriter(m_settings.m_byteOrder);
		streamWriter << uint8_t(ServiceMessageType::Ack);
		streamWriter << static_cast<uint32_t>(m_outputAcknowledgesSize);
		m_outputAcknowledgesSize = 0;
		m_outputSegments.push_front(streamWriter.GetBuffer().GetHolder()); // acknowledges has high priority, so pushing them to front!
	}

	if (IsOutputBufferEmpty()
		&& m_settings.m_hasLineTest
		&& m_lastTestActivity.GetElapsedTime() > m_settings.m_lineTestInterval
		&& m_framesQueueOutput.empty())
	{
		ByteOrderDataStreamWriter streamWriter(m_settings.m_byteOrder);
		streamWriter << uint8_t(ServiceMessageType::LineTest);
		m_outputSegments.push_back(streamWriter.GetBuffer().GetHolder());
	}

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

	while (!m_framesQueueOutput.empty())
	{
		SocketFrame::Ptr frontMsg;
		if (!m_framesQueueOutput.pop(frontMsg))
			throw std::logic_error("Invalid queue logic");

		ByteOrderDataStreamWriter streamWriter(m_settings.m_byteOrder);
		Syslogger(m_logContext, LOG_INFO) << "outgoung -> " << frontMsg;
		frontMsg->Write(streamWriter);

		const auto typeId = frontMsg->FrameTypeId();
		ByteArrayHolder buffer = streamWriter.GetBuffer().GetHolder();

		//Syslogger(m_logContext, LOG_INFO) << "buffer -> " << streamWriter.GetBuffer().ToHex();
		/// splitting onto segments.
		for (size_t offset = 0; offset < buffer.size(); offset += m_settings.m_segmentSize)
		{
			ByteArrayHolder bufferPart;
			size_t length = std::min(m_settings.m_segmentSize, buffer.size() - offset);
			if (m_settings.m_hasChannelTypes)
			{
				ByteOrderBuffer partStreamBuf(bufferPart);
				ByteOrderDataStreamWriter partStreamWriter(&partStreamBuf, m_settings.m_byteOrder);
				partStreamWriter << typeId << uint32_t(length);
			}
			bufferPart.ref().insert(bufferPart.ref().end(), buffer.ref().begin() + offset, buffer.ref().begin() + offset + length);
			m_outputSegments.push_back(bufferPart);
		}
	}


	while (!m_outputSegments.empty())
	{
		ByteArrayHolder frontSegment = m_outputSegments.front();

		const auto sizeForWrite = frontSegment.size();
		const auto maxSize = m_maxUnAcknowledgedSize - m_bytesWaitingAcknowledge;
		if (m_settings.m_hasAcknowledges && sizeForWrite > maxSize)
			break;

		m_outputSegments.pop_front();

		if (!m_channel->Write(frontSegment, sizeForWrite))
		{
			Syslogger(m_logContext, LOG_ERR) << "Write failed: sizeForWrite=" <<  sizeForWrite
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
	Syslogger(m_logContext, LOG_INFO) << "incoming <- " << incomingMessage;
	auto search = m_replyNotifiers.find(incomingMessage->m_replyToTransactionId);
	if (search != m_replyNotifiers.end())
	{
		auto callback = search->second;
		m_replyNotifiers.erase(search);
		callback(incomingMessage, rsSuccess);
	}
	else
	{
		auto tid = incomingMessage->m_transactionId;
		// frame factory succeeded, so it's safe do not check TypeId.
		m_frameReaders[incomingMessage->FrameTypeId()]->ProcessFrame(incomingMessage, [this, tid](SocketFrame::Ptr reply)
		{
			reply->m_replyToTransactionId = tid;
			QueueFrame(reply);
		});
	}
}

}
