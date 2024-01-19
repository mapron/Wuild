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
#include "MernelPlatform/Compression.hpp"

#include <cstdint>
#include <vector>
#include <string>
#include <functional>

namespace Wuild {

using CompressionInfo = Mernel::CompressionInfo;

class FileInfoPrivate;
/// Holds information about file on a disk.
class FileInfo {
    std::unique_ptr<FileInfoPrivate> m_impl;

public:
    static std::string LocatePath(const std::string& path);
    static std::string ToPlatformPath(std::string path);

    using PostProcessor = std::function<void(ByteArray&)>;

public:
    FileInfo(const FileInfo& rh);
    FileInfo& operator=(const FileInfo& rh);
    FileInfo(const std::string& filename = std::string());
    ~FileInfo();

    /// Set full file path in utf-8 encoding.
    void SetPath(const std::string& path);

    /// Get full file path in utf-8 encoding.
    std::string GetPath() const;

    /// Get file enclosing directory path.
    std::string GetDir(bool ensureEndSlash = false) const;

    /// Get fullname without directory. All extension included.
    std::string GetFullname() const;

    /// File name to first dot (excluding dot itself)
    std::string GetNameWE() const;

    /// All extensions as string, including first dot (if present)
    std::string GetFullExtension() const;

    /// Return short path on Win, full Path on other platfroms.
    std::string GetPlatformShortName() const;

    /// Read file from disk and compress its contents in memory.
    bool ReadCompressed(ByteArrayHolder& data, CompressionInfo compressionInfo);

    /// Write deflated memory data to file on disk uncompressed.
    bool WriteCompressed(const ByteArrayHolder& data, CompressionInfo compressionInfo, bool createTmpCopy = true, PostProcessor pp = {});

    /// Read whole file into buffer
    bool ReadFile(ByteArrayHolder& data);

    /// Write buffer to file
    bool WriteFile(const ByteArrayHolder& data, bool createTmpCopy = true);

    /// Check existence of file on disk.
    bool Exists();

    /// Returns file size in bytes for existent file, or 0 otherwise.
    size_t GetFileSize();

    /// Removes file. No error produced on failure.
    void Remove();

    /// Creates directories recursive
    void Mkdirs();

    /// Returns files list in directory, without parent path.
    StringVector GetDirFiles(bool sortByName = true);
};

/// Wrapper for filename string, which explicitly removes file in destructor.
class TemporaryFile : public FileInfo {
    TemporaryFile(const TemporaryFile&)            = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

public:
    TemporaryFile(const std::string& filename = std::string())
        : FileInfo(filename)
    {}
    ~TemporaryFile();
};

std::string GetCWD();
void        SetCWD(const std::string& cwd);

}
