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
#include <memory>
#include <vector>
#include <functional>

#include "TimePoint.h"

namespace Wuild {
class TcpEndPointPrivate;
class TcpEndPoint {
public:
    TcpEndPoint();
    TcpEndPoint(int port, const std::string& host = std::string());

    /// Set host and port information. No resolution performed until Resolve() call.
    void SetPoint(int port, const std::string& host = std::string());

    /// Creates internal data for host and port. If resolution already made, true returned.
    bool Resolve();

    /// Outputs host:port as string.
    std::string GetShortInfo() const;

    int                GetPort() const { return m_port; }
    const std::string& GetHost() const { return m_host; }
    const std::string& GetIpString() const { return m_ip; }

    const TcpEndPointPrivate& GetImpl() const { return *m_impl; }

private:
    std::shared_ptr<TcpEndPointPrivate> m_impl; //!< making possible to copy params.
    bool                                m_resolved   = false;
    bool                                m_errorShown = false;
    int                                 m_port       = 0;
    std::string                         m_host;
    std::string                         m_ip;
};

/// Parameters for tcp connection: client or server.
/// Contains information about host, port and timeouts.
struct TcpConnectionParams {
    TimePoint   m_readTimeout                  = 0.0;      //!< Scoket read timeout
    TimePoint   m_connectTimeout               = 1.0;      //!< Connection timeout
    size_t      m_recommendedRecieveBufferSize = 4 * 1024; //!< Buffer size is recommended for socket. If socket has lower size, buffer will be optionally increased (but not ought to)
    size_t      m_recommendedSendBufferSize    = 4 * 1024;
    TcpEndPoint m_endPoint;
};

struct TcpListenerParams : public TcpConnectionParams {
    int                      m_pendingListenConnections = 128;  //!< Maximum pending concurrent connections count
    bool                     m_reuseSockets             = true; //!< Reuse socket on listen, if possible. Helpful if program terminates and run again shortly.
    bool                     m_skipFailedConnection     = true; //!< Do not retry recreate listener on fail.
    std::vector<TcpEndPoint> m_whiteList;                       //!< This hosts are allowed to connect. If empty, any host is allowed.
    std::function<void()>    m_connectionFailureCallback;

    bool Resolve();
    void AddWhiteListPoint(int port, const std::string& host = std::string());
    bool IsAccepted(const std::string& host, std::string& allowed);
};

}
