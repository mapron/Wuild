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

#include "TcpSocket.h"

#include "Tcp_private.h"
#include "TcpConnectionParams_private.h"
#include "TcpListener.h"
#include "Syslogger.h"

#include <algorithm>
#include <vector>
#include <cstdint>
#include <memory>
#include <cstring>

//#define SOCKET_DEBUG  // for debugging.

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifdef TCP_SOCKET_WIN

void SocketEngineCheck()
{
    struct WSA_RAII {
        WSA_RAII()
        {
            int     ec;
            WSADATA wsa;

            if ((ec = WSAStartup(MAKEWORD(2, 0), &wsa)) != 0)
                Wuild::Syslogger(Wuild::Syslogger::Err) << "WIN32_SOCKET: winsock error: code " << ec;
        }
        ~WSA_RAII() { WSACleanup(); }
    };

    static WSA_RAII init;
}

#endif

namespace {
const size_t g_defaultBufferSize = 4 * 1024;
}

namespace Wuild {

#ifdef TCP_SOCKET_WIN
struct WindowClassContext {
    static LRESULT CALLBACK WinProcCallback(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
    {
        if (message == WM_NCCREATE)
            return true;

        return DefWindowProc(hwnd, message, wp, lp);
    }

    WindowClassContext()
        : atom(0)
        , className("wuild_internal_socket_hwnd")
    {
        WNDCLASS wc;
        wc.style         = 0;
        wc.lpfnWndProc   = WinProcCallback;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = GetModuleHandle(0);
        wc.hIcon         = 0;
        wc.hCursor       = 0;
        wc.hbrBackground = 0;
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = className;
        atom             = RegisterClass(&wc);
        if (!atom)
            Syslogger(Syslogger::Err) << "RegisterClass failed";
    }

    ~WindowClassContext()
    {
        if (atom) {
            UnregisterClass(className, GetModuleHandle(0));
        }
    }

    ATOM              atom;
    const char* const className;
};

class HwndWrapper {
    HWND       m_h;
    bool       m_stopped = false;
    std::mutex m_mutex;

public:
    static HwndWrapper* createInternal(const void* eventDispatcher, const void* obj)
    {
        static WindowClassContext ctx;
        std::string               winName(ctx.className);
        winName += std::to_string(intptr_t(obj));
        HWND wnd = CreateWindow(ctx.className,   // classname
                                winName.c_str(), // window name
                                0,               // style
                                0,
                                0,
                                0,
                                0,                  // geometry
                                HWND_MESSAGE,       // parent
                                0,                  // menu handle
                                GetModuleHandle(0), // application
                                0);                 // windows creation data.

        if (!wnd) {
            Syslogger(Syslogger::Warning) << "createInternal==0"
                                          << ", err=" << GetLastError();
            return new HwndWrapper(0);
        }

        SetWindowLongPtr(wnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(eventDispatcher));

        return new HwndWrapper(wnd);
    }
    HwndWrapper(HWND h)
        : m_h(h)
    {}
    ~HwndWrapper()
    {
        if (m_h)
            DestroyWindow(m_h);
    }
    void Stop()
    {
        std::unique_lock lock(m_mutex);
        if (!m_h || m_stopped)
            return;

        m_stopped = true;
        PostMessage(m_h, WM_QUIT, NULL, NULL);
    }
    HWND Get() const { return m_h; }
    bool Wait()
    {
        {
            std::unique_lock lock(m_mutex);
            if (!m_h || m_stopped)
                return false;
        }

        MSG m;
        GetMessage(&m, m_h, NULL, NULL);
        return true;
    }
};
#else
class HwndWrapper {
public:
    HwndWrapper() = default;
    void Stop() {}
};

#endif

std::atomic_bool TcpSocket::s_applicationInterruption(false);

TcpSocket::TcpSocket(const TcpConnectionParams& params)
    : m_params(params)
    , m_impl(new TcpSocketPrivate())
{
    SocketEngineCheck();
    m_logContext = params.m_endPoint.GetShortInfo();
    Syslogger(m_logContext) << "TcpSocket::TcpSocket() ";
}

TcpSocket::~TcpSocket()
{
    Disconnect();
}

IDataSocket::Ptr TcpSocket::Create(const TcpConnectionParams& params, TcpListener* pendingListener)
{
    auto sock = new TcpSocket(params);
    sock->SetListener(pendingListener);
    return IDataSocket::Ptr(sock);
}

bool TcpSocket::Connect()
{
    if (s_applicationInterruption)
        return false;

    if (m_state == ConnectionState::Success)
        return true;

    if (m_acceptedByListener) {
        if (m_state == ConnectionState::Fail)
            return false;

        if (m_state == ConnectionState::Pending && m_pendingListener->DoAccept(this)) {
            SetBufferSize();
            if (m_impl->SetBlocking(false)) {
                m_readPollCallbackInstall();
                m_state = ConnectionState::Success;
                return true;
            }
        }
        m_state = ConnectionState::Fail;
        return false;
    }
    Syslogger(m_logContext) << "Trying to connect...";

    if (m_impl->m_socket != INVALID_SOCKET) {
        Syslogger(m_logContext, Syslogger::Alert) << "Trying to reopen existing socket!";
        return false;
    }

    if (!m_params.m_endPoint.Resolve())
        return false;

    m_impl->m_socket = m_params.m_endPoint.GetImpl().MakeSocket();
    if (m_impl->m_socket == INVALID_SOCKET) {
        Syslogger(m_logContext, Syslogger::Err) << "socket creation failed.";
        Fail();
        return false;
    }

    SetBufferSize();
    if (!m_impl->SetNoSigPipe()) {
        Syslogger(m_logContext, Syslogger::Warning) << "Failed to disable SIGPIPE signal.";
    }
    if (!m_impl->SetBlocking(false)) {
        Fail();
        return false;
    }

    int cres = m_params.m_endPoint.GetImpl().Connect(m_impl->m_socket);
    if (cres < 0) {
        const auto err        = SocketGetLastError();
        const bool inProgress = SocketCheckConnectionPending(err);
        if (inProgress) {
            struct timeval timeout {};
            SET_TIMEVAL_US(timeout, m_params.m_connectTimeout);
            fd_set select_set;
            FD_ZERO(&select_set);
            FD_SET(m_impl->m_socket, &select_set);
            int       valopt;
            socklen_t valopt_len = sizeof(valopt);

            if (select(static_cast<int>(m_impl->m_socket + 1), nullptr, &select_set, nullptr, &timeout) <= 0 || !FD_ISSET(m_impl->m_socket, &select_set) || getsockopt(m_impl->m_socket, SOL_SOCKET, SO_ERROR, SOCK_OPT_ARG(&valopt), &valopt_len) < 0 || valopt) {
                Syslogger(m_logContext) << "Connection timeout.";
                Fail();
                return false;
            }
        } else {
            Syslogger(m_logContext) << "Connection failed. (" << cres << ") err=" << err;
            Fail();
            return false;
        }
    }
    m_readPollCallbackInstall();
    m_state = ConnectionState::Success;
    Syslogger(m_logContext) << "Connected.";
    return true;
}

void TcpSocket::Disconnect()
{
    if (m_hwnd)
        m_hwnd->Stop();
    m_socketReadPollThread.Stop();
    m_state = ConnectionState::Fail;
    if (m_impl->m_socket != INVALID_SOCKET) {
        Syslogger(m_logContext) << "TcpSocket::Disconnect() ";
        close(m_impl->m_socket);
        m_impl->m_socket = INVALID_SOCKET;
    }
}

bool TcpSocket::IsConnected() const
{
    return m_state == ConnectionState::Success;
}

bool TcpSocket::IsPending() const
{
    return m_state == ConnectionState::Pending;
}

IDataSocket::ReadState TcpSocket::Read(ByteArrayHolder& buffer)
{
    if (!IsConnected())
        return ReadState::Fail;

    if (!IsSocketReadReady())
        return ReadState::TryAgain;

    if (!SelectRead())
        return ReadState::TryAgain;

    size_t bufferInitialSize = buffer.size();
    (void) bufferInitialSize;
    char tmpbuffer[0x4000]; // 16k buffer. stack is rather cheap...
    int  recieved, totalRecieved = 0;
    int  iteration = 0;
    do {
        iteration++;
        recieved =
#ifdef TCP_SOCKET_WIN
            recv(m_impl->m_socket, tmpbuffer, sizeof(tmpbuffer), 0);
#else
            read(m_impl->m_socket, tmpbuffer, sizeof(tmpbuffer));
#endif
        if (recieved == 0) {
            if (iteration == 1) {
                Syslogger(m_logContext, Syslogger::Info) << "Connection closed.";
                Disconnect();
                return ReadState::Fail;
            }
            break;
        }

        if (recieved < 0) {
            const auto err = SocketGetLastError();
            if (SocketRWPending(err))
                break;
            Syslogger(m_logContext, Syslogger::Err) << "Disconnecting while Reading (" << recieved << ") err=" << err;
            Disconnect();
            return ReadState::Fail;
        }
        totalRecieved += recieved;
        buffer.ref().insert(buffer.ref().end(), tmpbuffer, tmpbuffer + recieved);

    } while (recieved == sizeof(tmpbuffer));
#ifdef SOCKET_DEBUG
    Syslogger(m_logContext) << "TcpSocket::Read: " << Syslogger::Binary(buffer.data() + bufferInitialSize, totalRecieved);
#endif

    return totalRecieved > 0 ? ReadState::Success : ReadState::TryAgain;
}

TcpSocket::WriteState TcpSocket::Write(const ByteArrayHolder& buffer, size_t maxBytes)
{
    if (m_impl->m_socket == INVALID_SOCKET)
        return WriteState::Fail;

    maxBytes = std::min(maxBytes, buffer.size());

    auto written = send(m_impl->m_socket, (const char*) (buffer.data()), static_cast<int>(maxBytes), MSG_NOSIGNAL);
    if (written < 0) {
        const auto err = SocketGetLastError();
        if (SocketRWPending(err))
            return WriteState::TryAgain;

        const int EPIPE_code = 32;
        Syslogger(m_logContext, err == EPIPE_code ? Syslogger::Info : Syslogger::Err) << "Disconnecting while Writing, (" << written << ") err=" << err;
        Disconnect();
        return WriteState::Fail;
    }

#ifdef SOCKET_DEBUG
    Syslogger(m_logContext) << "TcpSocket::Write: " << Syslogger::Binary(buffer.data(), written);
#endif
    return maxBytes == static_cast<size_t>(written) ? WriteState::Success : WriteState::Fail;
}

void TcpSocket::SetReadAvailableCallback(const std::function<void()>& cb)
{
#ifdef TCP_SOCKET_WIN
    m_readPollCallbackInstall = [this, cb] {
        std::shared_ptr<bool> onceCheck(new bool(false));
        m_socketReadPollThread.Exec([cb, onceCheck, this] {
            if (!*onceCheck) {
                *onceCheck = true;
                m_hwnd.reset(HwndWrapper::createInternal(nullptr, this));
                long event = FD_READ;
                int  res   = WSAAsyncSelect(m_impl->m_socket, m_hwnd->Get(), WM_USER, event);

                Syslogger(m_logContext, Syslogger::Info) << "WSAAsyncSelect=" << res << ", err=" << GetLastError();
            }
            {
                std::unique_lock<std::mutex> lock(m_awaitingMutex);
                m_awaitingCV.wait_for(lock, std::chrono::milliseconds(100), [this] {
                    return !!m_awaitingRead;
                });
            }

            TimePoint start(true);
            if (!m_hwnd->Wait())
                return false;

            {
                std::unique_lock<std::mutex> lock(m_awaitingMutex);
                if (m_awaitingRead)
                    cb();
                m_awaitingRead = false;
            }
            return false;
        });
    };
#endif
}

void TcpSocket::SetWaitForRead()
{
    std::unique_lock<std::mutex> lock(m_awaitingMutex);
    m_awaitingRead = true;
    m_awaitingCV.notify_one();
}

void TcpSocket::SetListener(TcpListener* pendingListener)
{
    if (!pendingListener)
        return;

    m_state              = ConnectionState::Pending;
    m_pendingListener    = pendingListener;
    m_acceptedByListener = true;
}

void TcpSocket::Fail()
{
    Syslogger(m_logContext) << "Disconnection cased by Fail()";
    Disconnect();
}

bool TcpSocket::IsSocketReadReady()
{
    if (m_impl->m_socket == INVALID_SOCKET)
        return false;

    fd_set set;
    FD_ZERO(&set);
    FD_SET(m_impl->m_socket, &set);

    struct timeval timeout  = { 0, 0 };
    int            selected = select(static_cast<int>(m_impl->m_socket + 1U), &set, nullptr, nullptr, &timeout);
    if (selected < 0) {
        Syslogger(m_logContext) << "Disconnect from IsSocketReadReady";
        Disconnect();
        return false;
    }
    return (selected != 0 && FD_ISSET(m_impl->m_socket, &set));
}

bool TcpSocket::SelectRead()
{
    if (m_impl->m_socket == INVALID_SOCKET)
        return false;

    bool   res = false;
    fd_set selected;
    FD_ZERO(&selected);
    FD_SET(m_impl->m_socket, &selected);

    struct timeval timeoutTV = { 0, 0 };

    res = select(static_cast<int>(m_impl->m_socket + 1), &selected, nullptr, nullptr, &timeoutTV) > 0 && FD_ISSET(m_impl->m_socket, &selected);
    return res;
}

void TcpSocket::SetBufferSize()
{
    m_recieveBufferSize = m_impl->GetRecieveBuffer();
    if (m_recieveBufferSize < m_params.m_recommendedRecieveBufferSize) {
        if (!m_impl->SetRecieveBuffer(static_cast<uint32_t>(m_params.m_recommendedRecieveBufferSize))) {
            Syslogger(m_logContext, Syslogger::Info) << "Failed to set recieve socket buffer size:" << m_params.m_recommendedRecieveBufferSize;
        }
        m_recieveBufferSize = m_impl->GetRecieveBuffer();
    }
    m_sendBufferSize = m_impl->GetSendBuffer();
    if (m_sendBufferSize < m_params.m_recommendedSendBufferSize) {
        if (!m_impl->SetSendBuffer(static_cast<uint32_t>(m_params.m_recommendedSendBufferSize))) {
            Syslogger(m_logContext, Syslogger::Info) << "Failed to set send socket buffer size:" << m_params.m_recommendedSendBufferSize;
        }
        m_sendBufferSize = m_impl->GetSendBuffer();
    }
}

bool TcpSocketPrivate::SetBlocking(bool blocking)
{
#if defined(TCP_SOCKET_WIN)
    u_long flags = blocking ? 0 : 1;
    if (ioctlsocket(m_socket, FIONBIO, &flags) != NO_ERROR) {
        Syslogger(Syslogger::Err) << "ioctlsocket(FIONBIO O_NONBLOCK) failed.";
        return false;
    }
#else
    long flags = fcntl(m_socket, F_GETFL, 0);
    if (flags < 0) {
        Syslogger(Syslogger::Err) << "fcntl(F_GETFL) failed.";
        return false;
    }
    if (!blocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;

    if (fcntl(m_socket, F_SETFL, flags) < 0) {
        Syslogger(Syslogger::Err) << "fcntl(F_SETFL O_NONBLOCK) failed.";
        return false;
    }
#endif
    return true;
}

bool TcpSocketPrivate::SetRecieveBuffer(uint32_t size)
{
    auto      valopt     = static_cast<int>(size);
    socklen_t valopt_len = sizeof(valopt);
    return setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, SOCK_OPT_ARG & valopt, valopt_len) == 0;
}

uint32_t TcpSocketPrivate::GetRecieveBuffer()
{
    int       valopt     = 0;
    socklen_t valopt_len = sizeof(valopt);
    if (getsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, SOCK_OPT_ARG & valopt, &valopt_len))
        return g_defaultBufferSize;

    return static_cast<uint32_t>(valopt);
}
bool TcpSocketPrivate::SetSendBuffer(uint32_t size)
{
    auto      valopt     = static_cast<int>(size);
    socklen_t valopt_len = sizeof(valopt);
    return setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, SOCK_OPT_ARG & valopt, valopt_len) == 0;
}

uint32_t TcpSocketPrivate::GetSendBuffer()
{
    int       valopt     = 0;
    socklen_t valopt_len = sizeof(valopt);
    if (getsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, SOCK_OPT_ARG & valopt, &valopt_len))
        return g_defaultBufferSize;

    return static_cast<uint32_t>(valopt);
}

bool TcpSocketPrivate::SetNoSigPipe()
{
#ifdef __APPLE__
    int value = 1;
    return setsockopt(m_socket, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value)) == 0;
#else
    return true;
#endif
}
}
