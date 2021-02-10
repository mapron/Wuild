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

#include "FileUtils.h"

#include "Compression.h"
#include "Syslogger.h"
#include "ThreadUtils.h"

#include <cassert>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <memory>
#include <streambuf>

#ifndef USE_GHC_STL
#include <filesystem>
namespace fs = std::filesystem;
using fserr = std::error_code;
#else
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
using fserr = std::error_code;
#endif

#ifdef _MSC_VER
#define strtoull _strtoui64
#define getcwd _getcwd
#define PATH_MAX _MAX_PATH
#endif

#if defined( _WIN32)
#include <windows.h>
#include <io.h>
#include <share.h>
#include <direct.h>
#else
#include <unistd.h>
#include <cerrno>
inline int GetLastError() { return errno; }
#endif

namespace {
const size_t CHUNK = 16384;
#ifdef _WIN32
static const char PATH_LIST_SEPARTOR = ';';
#else
const char PATH_LIST_SEPARTOR = ':';
#endif
}


namespace Wuild {

class FileInfoPrivate
{
public:
	fs::path m_path;
};

std::string FileInfo::LocatePath(const std::string &path)
{
	if (fs::path(path).is_absolute())
		return path;
	const std::string pathEnv = getenv("PATH");
	std::stringstream ss;
	ss.str(pathEnv);
	std::string item;
	while (std::getline(ss, item, PATH_LIST_SEPARTOR))
	{
		if (item.empty())
			continue;
		if (item[item.size()-1] != '/' && item[item.size()-1] != '\\')
			item += '/';
		if (fs::exists(item + path))
			return FileInfo(item + path).GetPlatformShortName();
	}
	return path;
}

std::string FileInfo::ToPlatformPath(std::string path)
{
#ifdef _WIN32
   std::replace(path.begin(), path.end(), '/', '\\');
   std::transform(path.begin(), path.end(), path.begin(), [](char c) { return ::tolower(c);});
#endif
   return path;
}

FileInfo::FileInfo(const FileInfo &rh)
    : m_impl(new FileInfoPrivate(*rh.m_impl))
{

}

FileInfo &FileInfo::operator =(const FileInfo &rh)
{
	m_impl = std::make_unique<FileInfoPrivate>(*rh.m_impl);
	return *this;
}

FileInfo::FileInfo(const std::string &filename)
	: m_impl(new FileInfoPrivate())
{
	m_impl->m_path = filename;
}

FileInfo::~FileInfo() = default;

void FileInfo::SetPath(const std::string &path)
{
	m_impl->m_path = path;
}

std::string FileInfo::GetPath() const
{
	return m_impl->m_path.u8string();
}

std::string FileInfo::GetDir(bool ensureEndSlash) const
{
	auto ret = m_impl->m_path.parent_path().u8string();
	if (!ret.empty() && ensureEndSlash)
		ret += '/';
	return ret;
}

std::string FileInfo::GetFullname() const
{
	return m_impl->m_path.filename().u8string();
}

std::string FileInfo::GetNameWE() const
{
	const auto name = this->GetFullname();
	const auto dot = name.find('.');
	return name.substr(0, dot);
}

std::string FileInfo::GetFullExtension() const
{
	const auto name = this->GetFullname();
	const auto dot = name.find('.');
	return name.substr( dot );
}

std::string FileInfo::GetPlatformShortName() const
{
#ifdef _WIN32
	std::string result = GetPath();
	fserr code;
	result = fs::canonical(result, code).u8string();
	long length = 0;

	// First obtain the size needed by passing NULL and 0.
	length = GetShortPathNameA(result.c_str(), nullptr, 0);
	if (length == 0)
		return result;

	// Dynamically allocate the correct size
	// (terminating null char was included in length)
	std::vector<char> buffer(length + 1);

	// Now simply call again using same long path.
	length = GetShortPathNameA(result.c_str(), buffer.data(), length);
	if (length == 0)
		return result;

	return ToPlatformPath(std::string(buffer.data(), length));
#else
	return GetPath();
#endif
}


bool FileInfo::ReadCompressed(ByteArrayHolder &data, CompressionInfo compressionInfo)
{
	std::ifstream inFile;
	inFile.open(GetPath().c_str(), std::ios::binary | std::ios::in);
	if (!inFile)
		return false;

	ByteArrayHolder uncompressedData;
	if (!ReadFile(uncompressedData))
		return false;

	try
	{
		CompressDataBuffer(uncompressedData, data, compressionInfo);
	}
	catch(std::exception &e)
	{
		Syslogger(Syslogger::Err) << "Error on reading:" << e.what() << " for " << GetPath();
		return false;
	}

	if (Syslogger::IsLogLevelEnabled(Syslogger::Debug))
		Syslogger() << "Compressed " << this->GetPath() << ": " << this->GetFileSize() << " -> " << data.size();

	return true;
}

bool FileInfo::WriteCompressed(const ByteArrayHolder & data, CompressionInfo compressionInfo, bool createTmpCopy)
{
	ByteArrayHolder uncompData;
	try
	{
		UncompressDataBuffer(data, uncompData, compressionInfo);
	}
	catch(std::exception &e)
	{
		Syslogger(Syslogger::Err) << "Error on uncompress:" << e.what() << " for " << GetPath();
		return false;
	}
	return this->WriteFile(uncompData, createTmpCopy);
}

bool FileInfo::ReadFile(ByteArrayHolder &data)
{
	FILE * f = fopen(GetPath().c_str(), "rb");
	if (!f)
		return false;

	ByteArray& dest = data.ref();

	unsigned char in[CHUNK];
	do {
		auto avail_in = fread(in, 1, CHUNK, f);
		if (!avail_in || ferror(f)) break;
		dest.insert(dest.end(), in, in + avail_in);
		if (feof(f)) break;

	} while (true);

	fclose(f);
	return true;
}

bool FileInfo::WriteFile(const ByteArrayHolder &data, bool createTmpCopy)
{
	const std::string originalPath = fs::absolute(m_impl->m_path).u8string();
	const std::string writePath = createTmpCopy ? originalPath + ".tmp" : originalPath;
	this->Remove();

	try
	{
#ifndef _WIN32
		std::ofstream outFile;
		outFile.open(writePath, std::ios::binary | std::ios::out);
		outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
		outFile.close();
#else
		auto fileHandle = CreateFileA((LPTSTR) writePath.c_str(), // file name
							   GENERIC_WRITE,        // open for write
							   0,                    // do not share
							   NULL,                 // default security
							   CREATE_ALWAYS,        // overwrite existing
							   FILE_ATTRIBUTE_NORMAL,// normal file
							   NULL);                // no template
		if (fileHandle == INVALID_HANDLE_VALUE)
			throw std::runtime_error("Failed to open file");

		size_t bytesToWrite = data.size(); // <- lossy

		size_t totalWritten = 0;
		do {
			auto blockSize = std::min(bytesToWrite, size_t(32 * 1024 * 1024));
			DWORD bytesWritten;
			if (!::WriteFile(fileHandle, data.data() + totalWritten, blockSize, &bytesWritten, NULL)) {
				if (totalWritten == 0) {
					// Note: Only return error if the first WriteFile failed.
					throw std::runtime_error("Failed to write data");
				}
				break;
			}
			if (bytesWritten == 0)
				break;
			totalWritten += bytesWritten;
			bytesToWrite -= bytesWritten;
		} while (totalWritten < data.size());

		if (!::CloseHandle(fileHandle))
			throw std::runtime_error("Failed to close file");

#endif
	}
	catch(std::exception &e)
	{
		Syslogger(Syslogger::Err) << "Error on writing:" << e.what() << " for " << writePath ;
		return false;
	}
	if (createTmpCopy)
	{
		fserr code;
		fs::rename(writePath, originalPath, code);
		if (code)
		{
			Syslogger(Syslogger::Err) << "Failed to rename " << writePath << " -> " << originalPath << " :" << GetLastError();
			return false;
		}
	}
	return true;
}

bool FileInfo::Exists()
{
	fserr code;
	return fs::exists(m_impl->m_path, code);
}

size_t FileInfo::GetFileSize()
{
	if (!Exists())
		return 0;

	fserr code;
	return fs::file_size(m_impl->m_path, code);
}

void FileInfo::Remove()
{
	fserr code;
	if (fs::exists(m_impl->m_path, code))
		fs::remove(m_impl->m_path, code);
}

void FileInfo::Mkdirs()
{
	fserr code;
	fs::create_directories(m_impl->m_path, code);
}

StringVector FileInfo::GetDirFiles(bool sortByName)
{
	StringVector res;
	for(const fs::directory_entry& it : fs::directory_iterator(m_impl->m_path))
	{
		 const fs::path& p = it.path();
		 if (fs::is_regular_file(p))
			res.push_back( p.filename().u8string() );
	}
	if (sortByName)
		std::sort(res.begin(), res.end());
	return res;
}

TemporaryFile::~TemporaryFile()
{
	this->Remove();
}

std::string GetCWD()
{
	std::vector<char> cwd;
	std::string workingDir;
	do
	{
		cwd.resize(cwd.size() + 1024);
		errno = 0;
	} while (!getcwd(&cwd[0], cwd.size()) && errno == ERANGE);
	if (errno != 0 && errno != ERANGE)
	{
		workingDir = ".";
	}
	else
	{
		workingDir = cwd.data();
		if (workingDir.empty())
			workingDir = ".";
	}
	std::replace(workingDir.begin(), workingDir.end(), '\\', '/');
	if (*workingDir.rbegin() != '/')
		workingDir += '/';

	return workingDir;
}

void SetCWD(const std::string &cwd)
{
#ifdef _WIN32
	_chdir(cwd.c_str());
#else
	if (chdir(cwd.c_str()) != 0)
		Syslogger(Syslogger::Err) << "Failed change current dir " << cwd;
#endif
}


}

