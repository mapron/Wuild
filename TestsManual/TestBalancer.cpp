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

#include <ToolBalancer.h>
#include <Application.h>

namespace {
	const std::string g_tool = "gcc";
	const size_t g_noIndex = std::numeric_limits<size_t>::max();
}

int main(int argc, char ** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "TestBalancer");
	using LoadVector = std::vector<uint16_t>;

	ToolBalancer balancer;
	balancer.SetSessionId(1);
	balancer.SetRequiredTools(StringVector(1, g_tool));
	TEST_ASSERT(balancer.GetFreeThreads() == 0);
	TEST_ASSERT(balancer.FindFreeClient(g_tool) == g_noIndex);

	ToolServerInfo info1, info2;
	info1.m_toolIds = StringVector(1, g_tool);
	info1.m_totalThreads = 8;
	info1.m_toolServerId = "test1";
	info2 = info1;
	info2.m_toolServerId = "test2";

	size_t index = 0;
	balancer.UpdateClient(info1, index);
	TEST_ASSERT(index == 0);
	balancer.UpdateClient(info1, index);
	TEST_ASSERT(index == 0);
	balancer.UpdateClient(info2, index);
	TEST_ASSERT(index == 1);

	TEST_ASSERT(balancer.GetFreeThreads() == 0);
	TEST_ASSERT((balancer.TestGetBusy() == LoadVector{0, 0}));

	balancer.SetClientActive(0, true);
	TEST_ASSERT(balancer.GetFreeThreads() == 8);
	balancer.SetClientActive(0, false);
	TEST_ASSERT(balancer.GetFreeThreads() == 0);
	balancer.SetClientActive(0, true);
	balancer.SetClientActive(1, true);
	TEST_ASSERT(balancer.GetFreeThreads() == 16);

	info2.m_connectedClients.resize(1);
	info2.m_connectedClients[0].m_usedThreads = 2;
	info2.m_connectedClients[0].m_sessionId  = 2;
	balancer.UpdateClient(info2, index);
	TEST_ASSERT((balancer.TestGetBusy() == LoadVector{0, 1}));

	index = balancer.FindFreeClient(g_tool);
	TEST_ASSERT(index == 0);
	balancer.StartTask(index);
	TEST_ASSERT((balancer.TestGetBusy() == LoadVector{1, 1}));

	index = balancer.FindFreeClient(g_tool);
	TEST_ASSERT(index == 0);
	balancer.StartTask(index);
	TEST_ASSERT((balancer.TestGetBusy() == LoadVector{2, 1}));

	balancer.StartTask( balancer.FindFreeClient(g_tool) );
	balancer.StartTask( balancer.FindFreeClient(g_tool) );

	index = balancer.FindFreeClient(g_tool);
	TEST_ASSERT(index == 1);
	balancer.StartTask(index);
	TEST_ASSERT((balancer.TestGetBusy() == LoadVector{3, 3}));

	std::cout << "OK\n";
	return 0;
}
