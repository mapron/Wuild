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

#include "AppUtils.h"

#include <CompilerProxyClient.h>
#include <StringUtils.h>

int main(int argc, char** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "WuildProxyClient", "proxy");

	ToolProxyClient::Config proxyConfig;
	if (!app.GetToolProxyServerConfig(proxyConfig))
		return 1;

	ToolProxyClient proxyClient;
	if (!proxyClient.SetConfig(proxyConfig))
		return 1;

	proxyClient.Start();

	proxyClient.RunTask(StringUtils::StringVectorFromArgv(argc, argv));

	return ExecAppLoop();
}
