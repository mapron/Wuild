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

#include "Compression.h"

#ifdef USE_ZSTD
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#endif

#include <fstream>
#include <sstream>

namespace Wuild {

void UncompressDataBuffer(const ByteArrayHolder& input, ByteArrayHolder& output, CompressionInfo compressionInfo)
{
    if (false) {
    }
#ifdef USE_ZSTD
    else if (compressionInfo.m_type == CompressionType::ZStd) {
        //size_t cSize;
        //void* const cBuff = loadFile_orDie(fname, &cSize);
        unsigned long long const rSize = ZSTD_findDecompressedSize(input.data(), input.size());
        if (rSize == ZSTD_CONTENTSIZE_ERROR)
            throw std::runtime_error("Data was not compressed by zstd.");
        else if (rSize == ZSTD_CONTENTSIZE_UNKNOWN)
            throw std::runtime_error("Original size unknown. Use streaming decompression instead.");

        output.resize(rSize);

        size_t const dSize = ZSTD_decompress(output.data(), rSize, input.data(), input.size());

        if (dSize != rSize)
            throw std::runtime_error("ZStd decompress failed:" + std::string(ZSTD_getErrorName(dSize)));
    }
#endif
    else if (compressionInfo.m_type == CompressionType::None) {
        output = input;
    } else {
        throw std::runtime_error("Unsupported compression type:" + std::to_string(static_cast<int>(compressionInfo.m_type)));
    }
}

void CompressDataBuffer(const ByteArrayHolder& input, ByteArrayHolder& output, CompressionInfo compressionInfo)
{
    if (false) {
    }
#ifdef USE_ZSTD
    else if (compressionInfo.m_type == CompressionType::ZStd) {
        size_t const cBuffSize = ZSTD_compressBound(input.size());
        output.resize(cBuffSize);

        size_t const cSize = ZSTD_compress(output.data(), cBuffSize, input.data(), input.size(), compressionInfo.m_level);
        if (ZSTD_isError(cSize))
            throw std::runtime_error("ZStd compress failed:" + std::string(ZSTD_getErrorName(cSize)));
        output.resize(cSize);
    }
#endif
    else if (compressionInfo.m_type == CompressionType::None) {
        output = input;
    } else {
        throw std::runtime_error("Unsupported compression type:" + std::to_string(static_cast<int>(compressionInfo.m_type)));
    }
}

}
