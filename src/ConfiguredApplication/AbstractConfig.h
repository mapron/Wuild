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

#include <map>

namespace Wuild {

/// Class for reading settings from ini file.
/// Format is very simple: key=value. Sections is not supported.
class AbstractConfig {
public:
    /// Set list in form ['key=value', 'key2=value2']
    void ReadCommandLine(const StringVector& args);
    bool ReadIniFile(const std::string& filename);

    /// No keys are present in config.
    bool empty() const;

    /// Value key is present in config.
    bool Exists(const std::string& group, const std::string& key) const;

    /// Get different value types. If value no present in group, it will be searched in default ("") group.
    int          GetInt(const std::string& group, const std::string& key, int defValue = 0) const;
    double       GetDouble(const std::string& group, const std::string& key, double defValue = 0) const;
    std::string  GetString(const std::string& group, const std::string& key, const std::string& defValue = "") const;
    StringVector GetStringList(const std::string& group, const std::string& key, const StringVector& defValue = StringVector()) const;
    bool         GetBool(const std::string& group, const std::string& key, bool defValue = false) const;

    std::string DumpAllValues() const;

private:
    void               SetArg(const std::string& group, const std::string& arg);
    const std::string* Find(const std::string& group, const std::string& key) const;

    using ValueGroup = std::map<std::string, std::string>;
    std::map<std::string, ValueGroup> m_data;
};

}
