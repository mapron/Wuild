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

#include "TimePoint.h"
#include "CommonTypes.h"

namespace Wuild
{
/// Abstract socket type. Used for sending/recieving over network.
class IDataSocket
{
public:
	using Ptr = std::shared_ptr<IDataSocket>;

	virtual ~IDataSocket() = default;

	/// Connect to target. If fails, returns false. Returns true on success or connection already exists. Synchronous call.
	virtual bool Connect () = 0;

	/// Breaks existing connection. Synchronous call.
	virtual void Disconnect () = 0;

	/// Returns true if socket is ready to send/recieve data.
	virtual bool IsConnected () const = 0;

	/// Connection succeded, but socket not ready.
	virtual bool IsPending() const = 0;

	/// Reads some data into ByteArray without blocking. Returns false if no data was read.
	virtual bool Read(ByteArrayHolder & buffer) = 0;

	/// Write data to socket. Returns false on error.
	virtual bool Write(const ByteArrayHolder & buffer, size_t maxBytes = size_t(-1)) = 0;
};
}
