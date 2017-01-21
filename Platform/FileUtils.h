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
#include "CommonTypes.h"

#include <stdint.h>
#include <vector>
#include <string>

namespace Wuild {

class FileInfoPrivate;
/// Holds information about file on a disk.
class FileInfo
{
	std::unique_ptr<FileInfoPrivate> m_impl;
public:
	FileInfo(const FileInfo& rh);
	FileInfo & operator = (const FileInfo & rh);
	FileInfo(const std::string & filename = std::string());
	~FileInfo();

	/// Set full file path in utf-8 encoding.
	void SetPath(const std::string & path);

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

	/// Read file from disk and compress its contents in memory.
	bool ReadGzipped(ByteArrayHolder & data, int level = 1);

	/// Write deflated memory data to file on disk uncompressed.
	bool WriteGzipped( const ByteArrayHolder & data);

	/// Read whole file into buffer
	bool ReadFile(ByteArrayHolder & data);

	/// Write buffer to file
	bool WriteFile(const ByteArrayHolder & data);

	/// Check existence of file on disk.
	bool Exists();

	/// Returns file size in bytes for existent file, or 0 otherwise.
	size_t FileSize();

	/// Removes file. No error produced on failure.
	void Remove();

	/// Creates directories recursive
	void Mkdirs();

	/// Returns files list in directory, without parent path.
	StringVector GetDirFiles(bool sortByName = true);
};

/// Wrapper for filename string, which explicitly removes file in destructor.
class TemporaryFile : public FileInfo
{
	TemporaryFile(const TemporaryFile& ) = delete;
	TemporaryFile& operator = (const TemporaryFile& ) = delete;

public:
	TemporaryFile(const std::string & filename = std::string()) : FileInfo(filename) {  }
	~TemporaryFile();
};

}
