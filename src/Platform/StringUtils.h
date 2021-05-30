/*
 * Copyright (C) 2017-2021 Smirnov Vladimir mapron1@gmail.com
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

#include "CommonTypes.h"

namespace Wuild { namespace StringUtils {

/// Joint string vector by char separator
std::string JoinString(const StringVector& list, char glue);

/// Remove whitespace from both ends
std::string Trim(const std::string& s);

/// Split string to std::vector using char delimiter. If trimEach is true, Trim() function applied to each element.
void SplitString(const std::string& str, StringVector& outList, char delim, bool trimEach = false, bool skipEmpty = false);

/// Copy argv values to StringVector. If skipFirstArg is true, [0] element is skipped.
StringVector StringVectorFromArgv(int argc, char** argv, bool skipFirstArg = true);

}}
