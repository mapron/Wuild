/*
 * Copyright (C) 2018 Smirnov Vladimir mapron1@gmail.com
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
/*#include "BenchmarkUtils.h"

int main(int argc, char** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "BenchmarkNetworking");
	auto args = app.GetRemainArgs();
	if (args.size() < 1)
	{
		Syslogger(Syslogger::Err) << "Usage: <server ip>";
		return 1;
	}

	TestService service;
	service.startClient(args[0]);
	TimePoint start(true);
	for (int i = 0; i< 50; i++)
		service.sendFile(1000000);
	service.waitForReplies();
	Syslogger(Syslogger::Notice) << "Taken time:" << start.GetElapsedTime().ToProfilingTime();

	return 0;
}*/

#include <uvw.hpp>
#include <memory>

#include <TimePoint.h>
#include <Syslogger.h>

using namespace Wuild;

void listen(uvw::Loop &loop) {
	std::shared_ptr<uvw::TcpHandle> tcp = loop.resource<uvw::TcpHandle>();

	tcp->once<uvw::ListenEvent>([](const uvw::ListenEvent &, uvw::TcpHandle &srv) {
		std::shared_ptr<uvw::TcpHandle> client = srv.loop().resource<uvw::TcpHandle>();

		client->on<uvw::CloseEvent>([ptr = srv.shared_from_this()](const uvw::CloseEvent &, uvw::TcpHandle &) { ptr->close(); });
		client->on<uvw::EndEvent>([](const uvw::EndEvent &, uvw::TcpHandle &client) { client.close(); });
		client->on<uvw::DataEvent>([](const uvw::DataEvent & e, uvw::TcpHandle &client) {
			Syslogger(Syslogger::Notice) << "Incoming:"<< e.length ;
			auto dataWrite = std::unique_ptr<char[]>(new char[e.length]);
			for (size_t i = 0; i < e.length; ++i)
				dataWrite.get()[i] = e.data.get()[i] ^ char(0x80);
			client.write(std::move(dataWrite), e.length);
		});

		srv.accept(*client);
		client->read();
	});

	tcp->bind("127.0.0.1", 4242);
	tcp->listen();
}

void conn(uvw::Loop &loop) {
	auto tcp = loop.resource<uvw::TcpHandle>();

	tcp->on<uvw::ErrorEvent>([](const uvw::ErrorEvent &, uvw::TcpHandle &) { /* handle errors */ });

	tcp->once<uvw::ConnectEvent>([](const uvw::ConnectEvent &, uvw::TcpHandle &tcp) {
		for (int i=0; i < 6; i++)
		{
			size_t len = 100000;
			auto dataWrite = std::unique_ptr<char[]>(new char[len]);
			for (size_t i = 0; i < len; ++i)
				dataWrite.get()[i] = char(i % 256);
			tcp.write(std::move(dataWrite), len);
		}
		tcp.close();
	});

	tcp->connect(std::string{"127.0.0.1"}, 4242);
}

int main(int argc, char** argv)
{
	auto loop = uvw::Loop::getDefault();
	listen(*loop);
	conn(*loop);
	loop->run();
}
