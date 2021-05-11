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

#include <string>
#include <vector>
#include <memory>
#include <stdint.h>

namespace Wuild {

using StringVector = std::vector<std::string>;
using ByteArray    = std::vector<uint8_t>;

/// Class for explicit sharing blob data.
class ByteArrayHolder {
    std::shared_ptr<ByteArray> p;

public:
    ByteArrayHolder()
        : p(new ByteArray())
    {}

    size_t           size() const { return p.get()->size(); }
    void             resize(size_t size) { return p.get()->resize(size); }
    uint8_t*         data() { return p.get()->data(); }
    const uint8_t*   data() const { return p.get()->data(); }
    ByteArray&       ref() { return *p.get(); }
    const ByteArray& ref() const { return *p.get(); }
};

}
