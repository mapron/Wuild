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

#include "TestUtils.h"

#include <SocketFrameService.h>
#include <ByteOrderStream.h>
#include <ThreadUtils.h>

#include <memory>

using namespace Wuild;

class TestFrame : public SocketFrameExt
{
public:
	using Ptr = std::shared_ptr<TestFrame>;

	static const uint8_t s_frameTypeId = s_minimalUserFrameId + 1;

	std::string m_text;
	static const uint64_t s_magic;

	TestFrame()
	{
		m_writeLength = true;
	}

	std::string ElidedText(int length = 64) const
	{
		if ((int)m_text.size() <= length )
			return m_text;

		return m_text.substr(0, length/2) + ".." + m_text.substr(m_text.size() - length/2);
	}

	void LogTo(std::ostream & os) const override
	{
		SocketFrame::LogTo(os);
		os << " " << ElidedText() << "[" << m_text.size() << "]";
	}

	uint8_t             FrameTypeId() const override { return s_frameTypeId;}

	State               ReadInternal(ByteOrderDataStreamReader &stream) override
	{
		uint64_t magick;
		stream >> magick;
		assert(magick == s_magic);
		stream >> m_text;
		return stOk;
	}

	State               WriteInternal(ByteOrderDataStreamWriter &stream) const override
	{
		stream << s_magic;
		stream << m_text;
		return stOk;
	}
};

class TestFrameReply : public TestFrame
{
public:
	using Ptr = std::shared_ptr<TestFrameReply>;

	static const uint8_t s_frameTypeId = s_minimalUserFrameId + 2;
	uint8_t             FrameTypeId() const override { return s_frameTypeId;}
};
const uint64_t TestFrame::s_magic= 0x0102030405060708ll;

class TestService
{
	std::unique_ptr<SocketFrameService> m_server;
	std::vector<SocketFrameHandler::Ptr> m_clients;

	TimePoint m_waitForConnectedClientsTimeout = TimePoint (1.0);
public:
	void setServer(int port);
	void addClient(const std::string& ip, int port);
	void sendHello(const std::string& hello, int repeats);
	void start();

};

using namespace Wuild;
namespace
{
const int segmentSize = 240;
const int textRepeats = 100000;
const int bufferSize = 128900;
const int testServicePort = 12345;
const std::string testHost = "localhost";
}

void TestService::setServer(int port)
{
	Syslogger(Syslogger::Info) << "Listening on:" << port;

	SocketFrameHandlerSettings settings;
	settings.m_recommendedRecieveBufferSize = bufferSize;
	settings.m_recommendedSendBufferSize    = bufferSize;
	settings.m_segmentSize = segmentSize;

	m_server = std::make_unique<SocketFrameService>( settings );
	m_server->AddTcpListener(port, testHost );
	m_server->RegisterFrameReader(SocketFrameReaderTemplate<TestFrame>::Create([](const TestFrame &inputMessage, SocketFrameHandler::OutputCallback outputCallback)
	{
		if (inputMessage.m_text.substr(0, 5) == "Hello")
		{
			TestFrameReply::Ptr response(new TestFrameReply());
			for (int i=0; i< textRepeats; ++i)
				response->m_text += "Good day!";
			outputCallback(response);
		}
	}));
	m_server->Start();
}

void TestService::addClient(const std::string& ip, int port)
{
	Syslogger() << "setClient " << ip  << ":" <<  port;
	SocketFrameHandlerSettings settings;
	settings.m_recommendedSendBufferSize = bufferSize;
	settings.m_recommendedRecieveBufferSize = bufferSize;
	settings.m_segmentSize = segmentSize;
	SocketFrameHandler::Ptr h(new SocketFrameHandler(settings));
	h->RegisterFrameReader(SocketFrameReaderTemplate<TestFrameReply>::Create([](const TestFrameReply &inputMessage, SocketFrameHandler::OutputCallback outputCallback)
	{
		int exitCode = 1;
		if (inputMessage.m_text.substr(0, 4) == "Good")
		{
			exitCode = 0;
		}
		Application::Interrupt(exitCode);
	}));
	h->SetLogContext("client");
	h->SetTcpChannel( ip, port);
	m_clients.push_back(h);
}

void TestService::sendHello(const std::string& hello, int repeats)
{
	for (const auto& client : m_clients)
	{
		TestFrame::Ptr frame(new TestFrame());
		for (int r = 0; r < repeats; ++r)
			frame->m_text += hello;
		client->QueueFrame(frame);
	}
}

void TestService::start()
{
	for (auto & client : m_clients)
		client->Start();
}


/*
 * Test for network communication. Outputs "OK" on success.
 */
int main(int argc, char** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "TestNetworking");

	ByteOrderBuffer buf;
	ByteOrderDataStreamReader streamReader(buf);
	ByteOrderDataStreamWriter streamWriter(buf);
	uint32_t test = 42;
	streamWriter << test;
	assert(buf.GetSize() == 4);
	buf.Reset();
	streamReader >> test;
	assert(test == 42);

	TestService service;
	service.setServer(testServicePort);
	usleep(100000);
	for (int i=0; i < 3; ++i)
	{
		service.addClient(testHost, testServicePort);
	}

	service.sendHello("Hello!", textRepeats);
	service.start();

	return ExecAppLoop(TestConfiguration::ExitHandler);
}
