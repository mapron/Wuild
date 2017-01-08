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

#include "IDataSocket.h"

namespace Wuild
{

/// Abstract port listener.
class IDataListener
{
public:
	using Ptr = std::shared_ptr<IDataListener>;
	virtual ~IDataListener() = default;

	/// Get first available incoming connection. To use result, you should call Connect() on it.
	virtual IDataSocket::Ptr GetPendingConnection() = 0;

	/// Start listen session on port. If fails to bind, returns false.
	virtual bool StartListen() = 0;

	/// Some descriptive string for listener
	virtual std::string GetLogContext() const = 0;
};

}
