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

#include "Compression.h"

#ifdef USE_ZLIB
#include <zlib.h>
#endif
#ifdef USE_LZ4
#include <lz4_stream.h>
#endif

#include <fstream>

namespace Wuild
{

namespace {
static const size_t CHUNK = 16384;
// TODO: on windows, recieving ERROR_SHARING_VIOLATION when attempting to rename temporary file.
static const size_t g_renameAttempts = 50;
static const int64_t g_renameUsleep = 100000;

static const size_t messageMaxBytes   = 1024;
static const size_t ringBufferBytes   = 1024 * 256 + messageMaxBytes;

#ifdef USE_ZLIB
/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
static int def(std::ifstream & source, std::vector<uint8_t> & dest, int level)
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
		source.read((char*)in, CHUNK);
		strm.avail_in = static_cast<uInt>(source.gcount());
		if (source.fail() && !source.eof()) {
			(void)deflateEnd(&strm);
			return Z_ERRNO;
		}
		flush = source.eof() ? Z_FINISH : Z_NO_FLUSH;
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
static int inf(const std::vector<uint8_t> & source, std::ofstream & dest)
{
	int ret;
	unsigned have;
	z_stream strm;
	//unsigned char in[CHUNK];
	unsigned char out[CHUNK];
	size_t remainSize = source.size();
	const uint8_t* sourceData = source.data();

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
			dest.write((const char*)(out), have);
			if ( dest.fail() ) {
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
#endif

struct ByteArrayHolderBufWriter : std::basic_streambuf<char, typename std::char_traits<char>> {
	 typedef std::basic_streambuf<char, typename std::char_traits<char>> base_type;
	 typedef typename base_type::int_type int_type;
	 typedef typename base_type::traits_type traits_type;

	ByteArrayHolderBufWriter(ByteArrayHolder& holder) : m_holder(holder) {}

	 virtual int_type overflow(int_type ch) {
		 if(traits_type::eq_int_type(ch, traits_type::eof()))
			 return traits_type::eof();
		 m_holder.ref().push_back(traits_type::to_char_type(ch));
		 return ch;
	 }

protected:
	ByteArrayHolder & m_holder;
};

struct ByteArrayHolderBufReader : std::streambuf
{
	ByteArrayHolderBufReader(const ByteArrayHolder & data) {
		this->setg((char*)data.data(), (char*)data.data(), (char*)data.data() + data.size());
	}
};

} // namespace

void ReadCompressedData(std::ifstream &inFile, ByteArrayHolder &data, CompressionInfo compressionInfo)
{
	if (false) {}
#ifdef USE_ZLIB
	else if (compressionInfo.m_type == CompressionType::Gzip)
	{
		auto result = def(inFile, data.ref(), compressionInfo.m_level);
		if (result != Z_OK)
			throw std::runtime_error("Gzip deflate failed:"  + std::to_string(result));
	}
#endif
#ifdef USE_LZ4
	else if (compressionInfo.m_type == CompressionType::LZ4)
	{
		ByteArrayHolderBufWriter outBuffer(data);
		std::ostream outBufferStream(&outBuffer);
		LZ4OutputStream lz4_out_stream(outBufferStream);

		std::copy(std::istreambuf_iterator<char>(inFile),
				  std::istreambuf_iterator<char>(),
				  std::ostreambuf_iterator<char>(lz4_out_stream));
		lz4_out_stream.close();
	}
#endif
	else if (compressionInfo.m_type == CompressionType::None)
	{
			ByteArrayHolderBufWriter outBuffer(data);
			std::ostream outBufferStream(&outBuffer);

			std::copy(std::istreambuf_iterator<char>(inFile),
					  std::istreambuf_iterator<char>(),
					  std::ostreambuf_iterator<char>(outBufferStream));
	}
	else
	{
		throw std::runtime_error("Unsupported compression type:" + std::to_string( static_cast<int>(compressionInfo.m_type)));
	}
}

void WriteCompressedData(std::ofstream & outFile, const ByteArrayHolder &data, CompressionInfo compressionInfo)
{
	if (false) {}
#ifdef USE_ZLIB
	else if (compressionInfo.m_type == CompressionType::Gzip)
	{
		auto infResult = inf(data.ref(), outFile);
		if (infResult != Z_OK)
			throw std::runtime_error("Gzip inflate failed:"  + std::to_string(infResult));
	}
#endif
#ifdef USE_LZ4
	else if (compressionInfo.m_type == CompressionType::LZ4)
	{
		ByteArrayHolderBufReader buffer(data);
		std::istream bufferStream(&buffer);
		LZ4InputStream lz4_in_stream(bufferStream);

		std::copy(std::istreambuf_iterator<char>(lz4_in_stream),
				  std::istreambuf_iterator<char>(),
				  std::ostreambuf_iterator<char>(outFile));

	}
#endif
	else if (compressionInfo.m_type == CompressionType::None)
	{
		ByteArrayHolderBufReader buffer(data);
		std::istream bufferStream(&buffer);

		std::copy(std::istreambuf_iterator<char>(bufferStream),
				  std::istreambuf_iterator<char>(),
				  std::ostreambuf_iterator<char>(outFile));

	}
	else
	{
		throw std::runtime_error("Unsupported compression type:" + std::to_string( static_cast<int>(compressionInfo.m_type)));
	}
}

}
