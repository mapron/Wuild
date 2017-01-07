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
		Syslogger(m_logContext, LOG_INFO) << "read/write Frames return false, " << ( m_retryConnectOnFail ? "waiting." : "stopping.");
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
	const size_t currentSize = m_BOBufferInput.GetHolder().size();
	if (!m_channel->Read(m_BOBufferInput.GetHolder()))
		return true; //nothing to read, it's not a error. Broken channel handled separately.

	m_lastTestActivity = m_lastSucceessfulRead = TimePoint(true);

	const size_t newSize = m_BOBufferInput.GetHolder().size();
	m_BOBufferInput.SetSize(newSize);
	ByteOrderDataStreamReader inputStream(&m_BOBufferInput, m_settings.m_byteOrder);

	m_doTestActivity = true;
	m_BOBufferInput.ResetRead();

	m_outputAcknowledgesSize += newSize - currentSize;

	do
	{
		ServiceMessageType mtype = DetermineFrameTypeInput();

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

			m_maxUnAcknowledgedSize = bufferSize * BUFFER_RATIO;
			if (version != m_versionId)
			{
				Syslogger(m_logContext, LOG_ERR) << "Remote version is  " << version << ", but mine is " << m_versionId;
				return false;
			}
			Syslogger(m_logContext)<< "Recieved buffer size = " << bufferSize << ", _MaxS=" << m_maxUnAcknowledgedSize  <<  ", remote time is " << m_remoteTimeDiffToPast.ToString() << " in past compare to me. (" << m_remoteTimeDiffToPast.GetUS() << " us)";

		}
		else
		{
			uint8_t mtypei = static_cast<uint8_t>(mtype);
			if (m_frameReaders.find(mtypei) == m_frameReaders.end())
			{
				m_BOBufferInput.Clear();
				Syslogger(m_logContext, LOG_ERR) << "MessageHandler: invalid type of ChannelMessage. " << int(mtype) << " "
											 << TimePoint(true).ToString() << " m_okFrames=" << m_okFrames;

				break;
			}
			SocketFrame::Ptr incoming(m_frameReaders[mtypei]->FrameFactory());
			try
			{
				state = incoming->Read(inputStream);
			}
			catch(std::exception & ex)
			{
				m_BOBufferInput.Clear();
				Syslogger(m_logContext, LOG_ERR) << "MessageHandler::readMessages exception: " << ex.what();
				return false;
			}
			if (state == SocketFrame::stIncomplete || m_BOBufferInput.EofRead())
			{
				break;
			}
			else if (state == SocketFrame::stBroken)
			{
				Syslogger(m_logContext, LOG_ERR) << "MessageHandler: broken message recieved. "
												<< TimePoint(true).ToString();

				m_BOBufferInput.Clear();
				break;
			}
			else if (state == SocketFrame::stOk)
			{ // если входной буфер успешно обработан - передаем в обработку дальше.
				m_okFrames++;
				PreprocessFrame(incoming);   // обработка собственно сообщения.
			}
		}

		m_BOBufferInput.RemoveFromStart(m_BOBufferInput.GetOffsetRead());// убираем прочитанное.

	} while (m_BOBufferInput.GetSize());

	return true;
}

bool SocketFrameHandler::WriteFrames()
{
	if (m_settings.m_hasAcknowledges && m_bytesWaitingAcknowledge >= m_maxUnAcknowledgedSize)
	{
		if (m_acknowledgeTimer.GetElapsedTime() > m_settings.m_acknowledgeTimeout)
		{
			Syslogger(m_logContext, LOG_ERR) << "Acknowledge not recieved! _lastWrite="
												  << m_acknowledgeTimer.ToString() << " now:"
												  << TimePoint(true).ToString() << " _acknowledgeTimeout="
												  << m_settings.m_acknowledgeTimeout.ToString();
			m_bytesWaitingAcknowledge = 0;
			return false;
		}
		return true;
	}

	if (IsOutputBufferEmpty() && m_settings.m_hasAcknowledges && m_outputAcknowledgesSize > m_settings.m_acknowledgeMinimalReadSize)
	{
		ByteOrderDataStreamWriter streamWriter(&m_BOBufferOutput, m_settings.m_byteOrder);
		streamWriter << uint8_t(ServiceMessageType::Ack);
		streamWriter << static_cast<uint32_t>(m_outputAcknowledgesSize);
		m_outputAcknowledgesSize = 0;
	}

	if (IsOutputBufferEmpty()
		&& m_settings.m_hasLineTest
		&& m_lastTestActivity.GetElapsedTime() > m_settings.m_lineTestInterval
		&& m_framesQueueOutput.empty())
	{
		ByteOrderDataStreamWriter streamWriter(&m_BOBufferOutput, m_settings.m_byteOrder);
		streamWriter << uint8_t(ServiceMessageType::LineTest);
	}

	if (IsOutputBufferEmpty() && m_setConnectionOptionsNeedSend)
	{
		m_setConnectionOptionsNeedSend = false;
		auto ch = std::dynamic_pointer_cast<TcpSocket>(m_channel);
		uint32_t size = ch ? ch->GetRecieveBufferSize() : 0;
		ByteOrderDataStreamWriter streamWriter(&m_BOBufferOutput, m_settings.m_byteOrder);
		streamWriter << uint8_t(ServiceMessageType::ConnOptions);
		streamWriter << size << m_versionId << TimePoint(true).GetUS();
	}


	if (IsOutputBufferEmpty())
	{
		while (!m_framesQueueOutput.empty())
		{
			SocketFrame::Ptr frontMsg;
			if (!m_framesQueueOutput.pop(frontMsg))
				return true; // should never happen.

			ByteOrderDataStreamWriter streamWriter(&m_BOBufferOutput, m_settings.m_byteOrder);
			if (m_settings.m_hasChannelTypes)
				streamWriter << frontMsg->FrameTypeId();

			Syslogger(m_logContext, LOG_INFO) << "outgoung -> " << frontMsg;
			frontMsg->Write(streamWriter);

			if (m_settings.m_hasAcknowledges && m_BOBufferOutput.GetSize() >= m_maxUnAcknowledgedSize )
				break;
		}
	}

	if (!IsOutputBufferEmpty())
	{
		auto holder = m_BOBufferOutput.GetHolder();
		auto sizeForWrite = m_BOBufferOutput.GetSize();
		auto maxSize = m_maxUnAcknowledgedSize - m_bytesWaitingAcknowledge;
		sizeForWrite = std::min(maxSize, static_cast<size_t>(sizeForWrite));
		if (!m_channel->Write(holder, sizeForWrite))
			return false;

		m_BOBufferOutput.RemoveFromStart(sizeForWrite);


		m_lastTestActivity = TimePoint(true);
		if (m_settings.m_hasAcknowledges)
		{
			m_acknowledgeTimer = m_lastTestActivity;
			 m_bytesWaitingAcknowledge += sizeForWrite;
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
	return m_BOBufferOutput.GetSize() == 0;
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

SocketFrameHandler::ServiceMessageType SocketFrameHandler::DetermineFrameTypeInput()
{
	if (!m_settings.m_hasChannelTypes)
		return ServiceMessageType::User;

	ByteOrderDataStreamReader streamReader(&m_BOBufferInput, m_settings.m_byteOrder);
	return SocketFrameHandler::ServiceMessageType(streamReader.ReadScalar<uint8_t>());
}

}
