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
#include "BenchmarkUtils.h"

#include <ByteOrderStreamTypes.h>

namespace Wuild {
FileFrame::FileFrame()
{
    m_writeLength = true;
}

void FileFrame::LogTo(std::ostream& os) const
{
    SocketFrame::LogTo(os);
    os << " file: [" << m_fileData.size();
    ;
}

SocketFrame::State FileFrame::ReadInternal(ByteOrderDataStreamReader& stream)
{
    stream >> m_processTime;
    stream >> m_fileData;
    return stOk;
}

SocketFrame::State FileFrame::WriteInternal(ByteOrderDataStreamWriter& stream) const
{
    stream << m_processTime;
    stream << m_fileData;
    return stOk;
}

namespace {
const int segmentSize     = 8192;
const int bufferSize      = 64 * 1024;
const int testServicePort = 12345;

void ProcessServerData(const ByteArrayHolder& input, ByteArrayHolder& output)
{
    output.resize(input.size());
    for (size_t i = 0; i < input.size(); ++i)
        output.data()[i] = input.data()[i] ^ uint8_t(0x80);
}
}

void TestService::startServer(std::function<void()> onInit)
{
    Syslogger(Syslogger::Info) << "Listening on:" << testServicePort;

    SocketFrameHandlerSettings settings;
    settings.m_recommendedRecieveBufferSize = bufferSize;
    settings.m_recommendedSendBufferSize    = bufferSize;
    settings.m_segmentSize                  = segmentSize;

    m_server = std::make_unique<SocketFrameService>(settings);
    m_server->AddTcpListener(testServicePort, "*");
    m_server->RegisterFrameReader(SocketFrameReaderTemplate<FileFrame>::Create([](const FileFrame& inputMessage, SocketFrameHandler::OutputCallback outputCallback) {
        FileFrame::Ptr response(new FileFrame());
        TimePoint      processStart(true);
        ProcessServerData(inputMessage.m_fileData, response->m_fileData);
        response->m_processTime = processStart.GetElapsedTime();
        outputCallback(response);
    }));
    m_server->SetHandlerDestroyCallback([](SocketFrameHandler*) {
        Syslogger(Syslogger::Notice) << "Interrupt!";
        Application::Interrupt();
    });
    m_server->SetHandlerInitCallback([onInit](SocketFrameHandler*) {
        onInit();
    });
    m_server->Start();
}

void TestService::startClient(const std::string& host)
{
    Syslogger() << "startClient " << host << ":" << testServicePort;
    SocketFrameHandlerSettings settings;
    settings.m_recommendedSendBufferSize    = bufferSize;
    settings.m_recommendedRecieveBufferSize = bufferSize;
    settings.m_segmentSize                  = segmentSize;
    SocketFrameHandler::Ptr h(new SocketFrameHandler(settings));
    h->RegisterFrameReader(SocketFrameReaderTemplate<FileFrame>::Create([this](const FileFrame& inputMessage, SocketFrameHandler::OutputCallback) {
        Syslogger(Syslogger::Info) << "process time:" << inputMessage.m_processTime.ToProfilingTime();
        std::unique_lock<std::mutex> lock(taskStateMutex);
        taskCount--;
        taskStateCond.notify_one();
    }));
    h->SetLogContext("client");
    h->SetTcpChannel(host, testServicePort);
    m_client = h;
    m_client->Start();
}

void TestService::sendFile(size_t size)
{
    FileFrame::Ptr request(new FileFrame());
    request->m_fileData.resize(size);
    for (size_t i = 0; i < size; ++i)
        request->m_fileData.data()[i] = uint8_t(i % 256);

    m_client->QueueFrame(request);
    std::unique_lock<std::mutex> lock(taskStateMutex);
    taskCount++;
}

void TestService::waitForReplies()
{
    std::unique_lock<std::mutex> lock(taskStateMutex);
    taskStateCond.wait(lock, [this] {
        return taskCount == 0;
    });
}

}
