/*
 * Copyright (C) 2018-2021 Smirnov Vladimir mapron1@gmail.com
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

#include <FileUtils.h>
#include <AppUtils.h>
#include <ConfiguredApplication.h>
#include <Application.h>
#include <InvocationRewriter.h>
#include <Syslogger.h>
#include <SocketFrameService.h>
#include <ByteOrderStream.h>
#include <ThreadUtils.h>

#include <memory>
#include <condition_variable>
#include <cassert>

namespace Wuild {

class FileFrame : public SocketFrameExt {
public:
    using Ptr = std::shared_ptr<FileFrame>;

    static const uint8_t s_frameTypeId = s_minimalUserFrameId + 1;

    ByteArrayHolder m_fileData;
    TimePoint       m_processTime;

    FileFrame();
    uint8_t FrameTypeId() const override { return s_frameTypeId; }

    void  LogTo(std::ostream& os) const override;
    State ReadInternal(ByteOrderDataStreamReader& stream) override;
    State WriteInternal(ByteOrderDataStreamWriter& stream) const override;
};

class TestService {
    std::unique_ptr<SocketFrameService> m_server;
    SocketFrameHandler::Ptr             m_client;

    TimePoint m_waitForConnectedClientsTimeout = TimePoint(1.0);

    size_t                  taskCount = 0;
    std::condition_variable taskStateCond;
    std::mutex              taskStateMutex;

public:
    void startServer(std::function<void()> onInit);
    void startClient(const std::string& host);
    void sendFile(size_t size);
    void waitForReplies();
};
}
