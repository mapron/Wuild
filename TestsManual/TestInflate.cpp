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

#include <FileUtils.h>
#include <Application.h>

void FillRandomBuffer(std::vector<uint8_t> & data, size_t size)
{
	data.resize(size);
	for (size_t i =0; i< size; ++i)
		data[i] = static_cast<uint8_t>((rand() %2)  );
}

int main(int argc, char ** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "TestInflate");

	CompressionInfo info;
	info.m_type = CompressionType::LZ4;

	ByteArrayHolder uncompressed, uncompressed2;
	FillRandomBuffer(uncompressed.ref(), 1000000);
	auto tmp = Application::Instance().GetTempDir();
	Syslogger(Syslogger::Notice) << "tmp=" << tmp;
	TemporaryFile f1(tmp + "/test1");
	TemporaryFile f2(tmp + "/test2");
	TemporaryFile f3(tmp + "/test3");

	f1.WriteFile(uncompressed);  //f1 contains uncompressed data
	f1.ReadFile(uncompressed2);

	TEST_ASSERT(uncompressed.ref() == uncompressed2.ref());

	ByteArrayHolder compressed;
	f1.ReadCompressed(compressed, info);
	f2.WriteFile(compressed);          //f2 contains compressed data

	ByteArrayHolder compressed2;
	f2.ReadFile(compressed2);

	TEST_ASSERT(compressed.ref() == compressed2.ref());
	bool res = f3.WriteCompressed(compressed2, info);

	TEST_ASSERT(res);
	auto f1size = f1.GetFileSize();
	auto f3size = f3.GetFileSize();
	TEST_ASSERT(f1size == f3size);

	ByteArrayHolder uncompressed3;
	f3.ReadFile(uncompressed3);        // f3 contains original data

	//for (size_t i =0; i< uncompressed.size() && i < uncompressed3.size(); ++i)
	//	TEST_ASSERT(uncompressed.data()[i] == uncompressed3.data()[i])

	TEST_ASSERT(uncompressed.ref() == uncompressed3.ref());
	std::cout << "OK\n";
	return 0;
}
