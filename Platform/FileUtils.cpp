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

#include <Syslogger.h>
#include <ThreadUtils.h>

#include <zlib.h>
#include <assert.h>
#include <stdio.h>
#include <algorithm>

#ifdef HAS_BOOST
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#define u8string string
using fserr = boost::system::error_code;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
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
#endif

namespace {
static const size_t CHUNK = 16384;
// TODO: on windows, recieving ERROR_SHARING_VIOLATION when attempting to rename temporary file.
static const size_t g_renameAttempts = 50;
static const int64_t g_renameUsleep = 100000;
}

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
static int def(FILE *source, std::vector<uint8_t> & dest, int level)
{
	int ret, flush;
	unsigned have;
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	/* allocate deflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = deflateInit(&strm, level);
	if (ret != Z_OK)
		return ret;

	/* compress until end of file */
	do {
		strm.avail_in = fread(in, 1, CHUNK, source);
		if (ferror(source)) {
			(void)deflateEnd(&strm);
			return Z_ERRNO;
		}
		flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
		strm.next_in = in;

		/* run deflate() on input until output buffer not full, finish
		   compression if all of source has been read in */
		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = deflate(&strm, flush);    /* no bad return value */
			assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			have = CHUNK - strm.avail_out;
			dest.insert(dest.end(), out, out + have);
		} while (strm.avail_out == 0);
		assert(strm.avail_in == 0);     /* all input will be used */

		/* done when last data in file processed */
	} while (flush != Z_FINISH);
	assert(ret == Z_STREAM_END);        /* stream will be complete */

	/* clean up and return */
	(void)deflateEnd(&strm);
	return Z_OK;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
static int inf(const std::vector<uint8_t> & source, FILE *dest)
{
	int ret;
	unsigned have;
	z_stream strm;
	//unsigned char in[CHUNK];
	unsigned char out[CHUNK];
	size_t remainSize = source.size();
	const uint8_t* sourceData = source.data();
	size_t written;

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
		return ret;

	/* decompress until deflate stream ends or end of file */
	do {
		strm.avail_in = std::min(remainSize, size_t(CHUNK));//fread(in, 1, CHUNK, source);
		remainSize -= strm.avail_in;

		if (strm.avail_in == 0)
			break;
		strm.next_in = (decltype(strm.next_in))sourceData;
		sourceData += strm.avail_in;

		/* run inflate() on input until output buffer not full */
		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}
			have = CHUNK - strm.avail_out;
			written = fwrite(out, 1, have, dest);
			if (written != have || ferror(dest)) {
				(void)inflateEnd(&strm);
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

namespace Wuild {

class FileInfoPrivate
{
public:
	fs::path m_path;
};

FileInfo::FileInfo(const FileInfo &rh)
	: m_impl(new FileInfoPrivate(*rh.m_impl))
{

}

FileInfo &FileInfo::operator =(const FileInfo &rh)
{
	m_impl.reset(new FileInfoPrivate(*rh.m_impl));
	return *this;
}

FileInfo::FileInfo(const std::string &filename)
	: m_impl(new FileInfoPrivate())
{
	m_impl->m_path = filename;
}

FileInfo::~FileInfo()
{

}

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


bool FileInfo::ReadGzipped( ByteArrayHolder &data, int level)
{
	FILE * f = fopen(GetPath().c_str(), "rb");
	if (!f)
		return false;
	bool result = true;
	if (def(f, data.ref(), level) != Z_OK)
		result = false;

	fclose(f);
	return result;
}

bool FileInfo::WriteGzipped(const ByteArrayHolder & data, bool createTmpCopy)
{
	const std::string originalPath = GetPath();
	const std::string writePath = createTmpCopy ? originalPath + ".tmp" : originalPath;
	this->Remove();
	bool result = true;
	{
		auto deleter = [&result, &writePath](FILE* f){
			if (f)
			{
				if (fclose(f) != 0)
				{
					Syslogger(Syslogger::Err) << "Failed to close " << writePath;
					result = false;
				}
			}
		};
		std::unique_ptr<FILE, decltype(deleter)> f(fopen(writePath.c_str(), "wb"), deleter);
		if (!f)
		{
			Syslogger(Syslogger::Err) << "Failed to open for write " << GetPath();
			return false;
		}

		if (inf(data.ref(), f.get()) != Z_OK)
		{
			Syslogger(Syslogger::Err) << "Failed to unzip data " << GetPath() << ", size = " << data.size();
			return false;
		}
	}

	if (!result)
		return result;

	if (createTmpCopy)
	{
		fserr code;
		for (size_t attempt = 0; attempt < g_renameAttempts; ++attempt)
		{
			fs::rename(writePath, originalPath, code);
			if (!code)
				break;
			usleep(g_renameUsleep);
		}

		if (code)
		{
			Syslogger(Syslogger::Err) << "Failed to rename " << GetPath() << " code:" << code;
			return false;
		}
	}
	return result;
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

bool FileInfo::WriteFile(const ByteArrayHolder &data)
{
	FILE * f = fopen(GetPath().c_str(), "wb");
	if (!f)
	{
		Syslogger(Syslogger::Err) << "Failed to write " << GetPath();
		return false;
	}

	bool result = fwrite(data.data(), data.size(), 1, f) > 0;
	if (fclose(f) == EOF)
	{
		Syslogger(Syslogger::Err) << "Failed to close " << GetPath();
		return false;
	}
	return result;
}

bool FileInfo::Exists()
{
	fserr code;
	return fs::exists(m_impl->m_path, code);
}

size_t FileInfo::FileSize()
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
	chdir(cwd.c_str());
}


}

