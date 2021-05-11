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

#include <TimePoint.h>

namespace Wuild
{
class CoordinatorClientConfig : public IConfig
{
public:
	enum class Redundance { All, Any };

	std::string m_logContext;
	bool m_enabled = true;
	StringVector m_coordinatorHost;
	int m_coordinatorPort = 0;
	TimePoint m_sendInfoInterval;
	Redundance m_redundance = Redundance::All;

	bool Validate(std::ostream * errStream = nullptr) const override;
};
}
