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
#include "IConfig.h"
#include "CoordinatorClientConfig.h"

#include <FileUtils.h>

namespace Wuild
{
class RemoteToolServerConfig : public IConfig
{
public:
	std::string m_serverName;
	std::string m_listenHost;
	StringVector m_hostsWhiteList; //!< List of hostnames which allowed to connect. If empty, any host allowed.
	int m_listenPort = 0;
	int m_threadCount = 1;
	CoordinatorClientConfig m_coordinator;
	CompressionInfo m_compression;
	bool m_useClientCompression = true;
	bool Validate(std::ostream * errStream = nullptr) const override;
};
}
