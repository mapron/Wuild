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

#pragma once

#include "SocketFrame.h"

#include "ThreadUtils.h"
#include "ThreadLoop.h"
#include "IDataSocket.h"
#include "ByteOrderBuffer.h"
#include "Syslogger.h"

#include <functional>
#include <atomic>
#include <map>

namespace Wuild {

inline Syslogger& operator<<(Syslogger& logger, const SocketFrame::Ptr& frame)
{
    std::ostringstream os;
    if (frame)
        frame->LogTo(os);
    else
        os << "nullptr";
    logger << os.str();
    return logger;
}

class SocketFrameHandler;
/// Settings for FrameHandler - network channel layer
struct SocketFrameHandlerSettings {
    SocketFrameHandlerSettings();

    uint32_t m_channelProtocolVersion = 1; //!< Channel protocol version. Client and server must have the same.

    size_t  m_acknowledgeMinimalReadSize = 100; //!< minimal bytes read without ack; must be greater than 5.
    uint8_t m_byteOrder;                        //!< network channel byte order. Default is big-endian.

    TimePoint m_clientThreadSleep = TimePoint(0.001); //!< thread usleep value.
    TimePoint m_mainThreadSleep   = TimePoint(0.001); //!< thread usleep value for FrameHandlerService.

    TimePoint m_channelActivityTimeout    = TimePoint(10.0); //!< After this time, channel with no read event will be dead.
    TimePoint m_acknowledgeTimeout        = TimePoint(10.0); //!< After this time, unacknowledged data send will stated failed.
    TimePoint m_lineTestInterval          = TimePoint(3.0);  //!< If no channel activity for this time, line test frame will be send.
    TimePoint m_afterDisconnectWait       = TimePoint(10.0); //!< If channel was disconnected, connect retry will be after that time.
    TimePoint m_replyTimeoutCheckInterval = TimePoint(1.0);  //!< How often check for timeouted requests.
    TimePoint m_connStatusInterval        = TimePoint(1.0);

    TimePoint m_tcpReadTimeout               = TimePoint(0.0); //!< Read timeout for underlying physical channel.
    size_t    m_recommendedRecieveBufferSize = 4 * 1024;       //!< Recommended TCP-buffer size.
    size_t    m_recommendedSendBufferSize    = 4 * 1024;       //!< Recommended TCP-buffer size.
    size_t    m_segmentSize                  = 240;            //!< Maximal length of channel layer frame.
                                                               // Network features used by FrameHandler
    bool m_hasAcknowledges = true;                             //!< Acknowledges
    bool m_hasLineTest     = true;                             //!< Test frames
    bool m_hasConnOptions  = true;                             //!< Send connect options after connection established.
    bool m_hasChannelTypes = true;                             //!< Use frame type marker in stream. Without that, all frames should have SocketFrame::s_minimalUserFrameId id.
    bool m_hasConnStatus   = false;

    int m_writeFailureLogLevel = Syslogger::Err;
};

/**
 * \brief Handler for network channel layer. Manages communication using SocketFrames
 *
 * Handler performs:
 * -Reading socket data, determination of types, allocating right class objects (using registered IFrameReaders)
 * -Working with acknowledges and line tests;
 * -Managing frame queue and output write buffer.
 */
class SocketFrameHandler final {
public:
    enum class ReplyState
    {
        Success,
        Error,
        Timeout
    };

    enum class QuantResult
    {
        JobDone,
        NeedSleep,
        Interrupt,
    };

    using Ptr                   = std::shared_ptr<SocketFrameHandler>;
    using StateNotifierCallback = std::function<void(bool)>;
    using ReplyNotifier         = std::function<void(SocketFrame::Ptr, ReplyState, const std::string&)>;
    using OutputCallback        = std::function<void(SocketFrame::Ptr)>;

    class IFrameReader {
    public:
        using Ptr               = std::shared_ptr<IFrameReader>;
        virtual ~IFrameReader() = default;

        /// Uniue identifier of frame type for one handler. Should be at least==SocketFrame::s_minimalUserFrameId.
        virtual uint8_t FrameTypeId() const = 0;

        /// Creates new frame.
        virtual SocketFrame::Ptr FrameFactory() const = 0;

        /// This function specify handling of incoming frames. In function, call outputCallback() to enqueue new frames as reply in hadler.
        virtual void ProcessFrame(SocketFrame::Ptr incomingMessage, OutputCallback outputCallback) = 0;
    };

    struct ConnectionStatus {
        uint16_t uniqueRepliesQueued;
    };
    using ConnectionStatusCallback = std::function<void(const ConnectionStatus&)>;

public:
    explicit SocketFrameHandler(int threadId, const SocketFrameHandlerSettings& settings = SocketFrameHandlerSettings());
    explicit SocketFrameHandler(const SocketFrameHandlerSettings& settings = SocketFrameHandlerSettings());
    ~SocketFrameHandler();

    // Process cycle management:
    /// For tcp listener accepted connections, we should ignore connection failure. So, pass retry = false for this behaviour.
    void SetRetryConnectOnFail(bool retry);

    /// Runs new thread. Returns immediately.
    void Start();

    /// Stops process thread.
    void Stop();

    /// Stop quant function.
    void Cancel();

    QuantResult Quant();

    /// Check loop and connection status.
    bool IsActive() const;

    // Underlying channel functions:
    /// Set tcp client
    void SetTcpChannel(const std::string& host, int port, TimePoint connectionTimeout = 1.0);

    /// Set abscract channel. Used for accepted connections.
    void SetChannel(IDataSocket::Ptr channel);

    void DisconnectChannel();

    /// callback will be called when underlying state changes. It also will called on initial state.
    void SetChannelNotifier(StateNotifierCallback stateNotifier);

    /// register watcher for connection status change.
    void SetConnectionStatusNotifier(ConnectionStatusCallback callback);

    // Application logic:
    ///  Adding new frame to queue. If replyNotifier is set, it will called instead of IFrameReader::ProcessFrame, when reply arrived or failure occurs.
    void QueueFrame(const SocketFrame::Ptr& message, const ReplyNotifier& replyNotifier = ReplyNotifier(), TimePoint timeout = TimePoint());

    /// Register new frame reader. FrameId should start from s_minimalUserFrameId!
    void RegisterFrameReader(const IFrameReader::Ptr& reader);

    //Logging:
    void SetLogContext(const std::string& context);
    void UpdateLogContext();

    int GetThreadId() const;

protected:
    enum class ServiceMessageType
    {
        None,
        Ack,
        LineTest,
        ConnOptions,
        ConnStatus,
        User = SocketFrame::s_minimalUserFrameId
    };

    enum class ConsumeState
    {
        Ok,
        Broken,
        Incomplete,
        FatalError
    };

    enum class ConnectionState
    {
        Pending,
        Ok,
        Failed
    };

    struct AliveStateHolder {
        using Ptr      = std::shared_ptr<AliveStateHolder>;
        bool m_isAlive = true;
    };

    class ReplyManager {
        std::mutex                        m_mutex;
        std::map<uint64_t, ReplyNotifier> m_replyNotifiers;
        std::map<uint64_t, TimePoint>     m_timeouts;

    public:
        void          ClearAndSendError();
        void          AddNotifier(uint64_t id, ReplyNotifier callback, TimePoint timeout);
        void          CheckTimeouts(const std::string& extraInfo);
        ReplyNotifier TakeNotifier(uint64_t id);
    };

protected:
    void         SetConnectionState(ConnectionState connectionState);
    QuantResult  ReadFrames();
    ConsumeState ConsumeReadBuffer();
    ConsumeState ConsumeFrameBuffer();
    QuantResult  WriteFrames();
    bool         CheckConnection() const;
    bool         CheckAndCreateConnection();
    bool         IsOutputBufferEmpty();

    void             PreprocessFrame(const SocketFrame::Ptr& incomingMessage);
    ConnectionStatus CalculateStatus();

protected:
    const int m_threadId;

    bool            m_retryConnectOnFail  = true;
    ConnectionState m_prevConnectionState = ConnectionState::Pending;

    StateNotifierCallback     m_stateNotifier;
    ConnectionStatusCallback  m_connStatusNotifier;
    std::atomic_uint_fast64_t m_transaction{ 0 };

    IDataSocket::Ptr                 m_channel;
    const SocketFrameHandlerSettings m_settings;

    ByteOrderBuffer m_readBuffer;
    ByteOrderBuffer m_frameDataBuffer;
    struct SegmentInfo {
        ByteArrayHolder    data;
        size_t             transaction = 0;
        ServiceMessageType type() const { return ServiceMessageType(data.data()[0]); }
        SegmentInfo() = default;
        SegmentInfo(const ByteArrayHolder& d)
            : data(d){};
    };
    std::deque<SegmentInfo> m_outputSegments;
    ServiceMessageType      m_pendingReadType = ServiceMessageType::None;

    ThreadSafeQueue<SocketFrame::Ptr>    m_framesQueueOutput;
    size_t                               m_outputAcknowledgesSize = 0;
    ReplyManager                         m_replyManager;
    std::map<uint8_t, IFrameReader::Ptr> m_frameReaders;

    uint8_t m_outputLoadPercent     = 0;
    size_t  m_maxUnAcknowledgedSize = 0;

    size_t    m_bytesWaitingAcknowledge = 0;
    TimePoint m_lastSucceessfulRead;
    TimePoint m_lastSucceessfulWrite;
    TimePoint m_acknowledgeTimer;
    TimePoint m_lastTestActivity;
    TimePoint m_lastTimeoutCheck;
    TimePoint m_lastConnStatusSend;
    bool mutable m_doTestActivity               = false;
    bool mutable m_setConnectionOptionsNeedSend = false;
    TimePoint m_remoteTimeDiffToPast;
    bool      m_lineTestQueued = false;

    std::string           m_logContextAdditional;
    std::string           m_logContext;
    ThreadLoop            m_thread;
    AliveStateHolder::Ptr m_aliveHolder;
};

/// Convenience FrameReader creator. FrameType is SocketFrame successor.
/// Requirenments: FrameType must have static uint8_t s_frameTypeId field.
/// OutputCallback paramenter in constructor is optional.
template<typename FrameType>
class SocketFrameReaderTemplate : public SocketFrameHandler::IFrameReader {
public:
    using Callback = std::function<void(const FrameType&, SocketFrameHandler::OutputCallback)>;
    SocketFrameReaderTemplate(const Callback& callback = Callback())
        : m_callback(callback)
    {}

    static SocketFrameHandler::IFrameReader::Ptr Create(const Callback& callback = Callback())
    {
        return SocketFrameHandler::IFrameReader::Ptr(new SocketFrameReaderTemplate(callback));
    }

    SocketFrame::Ptr FrameFactory() const override { return SocketFrame::Ptr(new FrameType()); }
    uint8_t          FrameTypeId() const override { return FrameType::s_frameTypeId; }
    void             ProcessFrame(SocketFrame::Ptr incomingMessage, SocketFrameHandler::OutputCallback outputCallback) override
    {
        if (m_callback)
            m_callback(dynamic_cast<const FrameType&>(*incomingMessage.get()), outputCallback);
    }

private:
    Callback m_callback;
};

}
