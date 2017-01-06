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

#include "ByteOrderStream_macro.h"
#include "ByteOrderBuffer.h"

#include <deque>
#include <type_traits>

namespace Wuild {
/**
 * @brief Data stream which can have any byte order.
 *
 * Operates on ByteOrderBuffer. You can specify byte order for stream in constructor.
 *
 * General usage:
 *
 * ByteOrderDataStreamWriter stream; // creating stream;
 * stream << uint32_t(42);           // Writing scalar data;
 * stream << std::string("Foo bar"); // Writing binary strings;
 *
 * stream.GetBuffer().begin(), stream.GetBuffer().size()  // retrieving serialized data.
 */
class ByteOrderDataStream
{
public:
    /// Creates stream object. ProtocolMask sets actual stream byte order: for byte, words and dwords. Default is Big Endian for all.
    inline ByteOrderDataStream(ByteOrderBuffer* buf = nullptr, uint_fast8_t ProtocolMask = CreateByteorderMask(ORDER_BE, ORDER_BE, ORDER_BE))
        : m_bufOwner(false), m_buf(buf)
    {
        if (!m_buf)
        {
            m_buf = new ByteOrderBuffer();
            m_bufOwner = true;
        }
        SetMask(ProtocolMask);
    }
    inline ByteOrderDataStream(uint_fast8_t ProtocolMask)
    {
        m_buf = new ByteOrderBuffer();
        m_bufOwner = true;
        SetMask(ProtocolMask);
    }

    ~ByteOrderDataStream()
    {
        if (m_bufOwner)
            delete m_buf;
    }

    inline       ByteOrderBuffer& GetBuffer()       { return *m_buf; }
    inline const ByteOrderBuffer& GetBuffer() const { return *m_buf; }

    /// Set byteorder settings for stream. To create actual value, use CreateByteorderMask.
    inline void SetMask(uint_fast8_t protocolMask)
    {
        protocolMask &= 7;
        m_maskInt64  = (HOST_BYTE_ORDER_INT   | HOST_WORD_ORDER_INT   << 1 |HOST_DWORD_ORDER_INT64  << 2 ) ^ protocolMask;
        m_maskDouble = (HOST_BYTE_ORDER_FLOAT | HOST_WORD_ORDER_FLOAT << 1 |HOST_DWORD_ORDER_DOUBLE << 2 ) ^ protocolMask;
        m_maskInt32 = m_maskInt64 & 3;
        m_maskInt16 = m_maskInt32 & 1;
        m_maskInt8  = 0;
        m_maskFloat = m_maskDouble & 3;
    }
    /// Creating mask description of byteorder. endiannes8 - byte order in word, endiannes16 - word order in dword, endiannes32 - dword order in qword.
    static inline uint_fast8_t CreateByteorderMask(uint_fast8_t endiannes8, uint_fast8_t endiannes16, uint_fast8_t endiannes32)
    {
        return endiannes8 | (endiannes16 << 1) | (endiannes32 << 2);
    } 
    template<typename T>
    inline uint_fast8_t GetTypeMask() const
    {
        return 0;
    }

protected:
    ByteOrderDataStream(const ByteOrderDataStream& another) = delete;
    ByteOrderDataStream(ByteOrderDataStream&& another) = delete;

    ByteOrderDataStream & operator = (const ByteOrderDataStream& another) = delete;
    ByteOrderDataStream & operator = (ByteOrderDataStream&& another) = delete;

    ByteOrderBuffer* m_buf = nullptr;
    uint_fast8_t m_maskInt8;
    uint_fast8_t m_maskInt16;
    uint_fast8_t m_maskInt32;
    uint_fast8_t m_maskInt64;
    uint_fast8_t m_maskFloat;
    uint_fast8_t m_maskDouble;
    bool m_bufOwner = false;
};

class ByteOrderDataStreamReader : public ByteOrderDataStream
{
public:
    using ByteOrderDataStream::ByteOrderDataStream;

    inline bool EofRead() const {  return m_buf->EofRead(); }

    template<typename T>
    inline ByteOrderDataStreamReader& operator >> (T &data)
    {
        constexpr size_t size = sizeof(T);
        static_assert(std::is_arithmetic<T>::value, "Only scalar data streaming is allowed.");
        const uint8_t* bufferP = m_buf->PosRead(size);
        if (!bufferP)
            return *this;

        read<size>(reinterpret_cast<uint8_t*>(&data), bufferP, this->GetTypeMask<T>());
        m_buf->MarkRead(size);
        return *this;
    }
    template <class T>
    inline T ReadScalar() {  T ret = T(); *this >> ret; return ret; }

    template<typename T>
    inline ByteOrderDataStreamReader& operator >> (std::vector<T> &data)
    {
        uint32_t size = 0;
        *this >> size;
        data.resize(size);
        for (auto & element : data)
            *this >> element;
        return *this;
    }

    template<typename T>
    inline ByteOrderDataStreamReader& operator >> (std::deque<T> &data)
    {
        uint32_t size = 0;
        *this >> size;
        data.resize(size);
        for (auto & element : data)
            *this >> element;
        return *this;
    }

    bool ReadPascalString(std::string & str)
    {
        size_t size = this->ReadScalar<size_t>();
        if (this->EofRead())
            return false;
        str.resize(size);

        if (!ReadBlock((uint8_t *)str.data(), size))
            return false;

        return !EofRead();
    }
    bool ReadBlock(uint8_t * data, ptrdiff_t size)
    {
        const uint8_t * start = m_buf->PosRead(size);
        if (!start)
            return false;

        memcpy(data, start, size);
        m_buf->MarkRead(size);
        m_buf->CheckRemain(0);
        return !EofRead();
    }
private:
    template<size_t bytes>
    inline void read(uint8_t* ,const uint8_t*,uint_fast8_t ) const{ static_assert(bytes <= 8, "Unknown size");}

};

class ByteOrderDataStreamWriter : public ByteOrderDataStream
{
public:
    using ByteOrderDataStream::ByteOrderDataStream;

    template<typename T>
    inline ByteOrderDataStreamWriter& operator << (const T &data)
    {
        constexpr size_t size = sizeof(T);
        static_assert(std::is_arithmetic<T>::value, "Only scalar data streaming is allowed.");

        uint8_t* bufferP = m_buf->PosWrite(size);
        if (!bufferP)
            return *this;

        write<size>(reinterpret_cast<const uint8_t*>(&data), bufferP, this->GetTypeMask<T>());
        m_buf->MarkWrite(size);
        return *this;
    }

    template<typename T>
    inline ByteOrderDataStreamWriter& operator << (const std::vector<T> &data)
    {
        uint32_t size = static_cast<uint32_t>(data.size());
        *this << size;
        for (auto & element : data)
            *this << element;
        return *this;
    }
    template<typename T>
    inline ByteOrderDataStreamWriter& operator << (const std::deque<T> &data)
    {
        uint32_t size = static_cast<uint32_t>(data.size());
        *this << size;
        for (auto & element : data)
            *this << element;
        return *this;
    }

    template<typename T>
    inline void WriteToOffset (const T &data, ptrdiff_t writeOffset);

    /// Read/write strings with size.
    bool WritePascalString(const std::string& str)
    {
        *this << str.size();

        if (!WriteBlock(reinterpret_cast<const uint8_t *>(str.data()), str.size()))
            return false;

        return true;
    }
    /// Read/write raw data blocks.
    bool WriteBlock(const uint8_t * data, ptrdiff_t size)
    {
        uint8_t * start = m_buf->PosWrite(size);
        if (!start)
            return false;

        memcpy(start, data, size);
        m_buf->MarkWrite(size);
        m_buf->CheckRemain(0);
        return true;
    }
private:
    template<size_t bytes>
    inline void write(const uint8_t* ,uint8_t* ,uint_fast8_t ) const{static_assert(bytes <= 8, "Unknown size"); }

};

template<> inline uint_fast8_t ByteOrderDataStream::GetTypeMask<uint64_t>() const { return m_maskInt64; }
template<> inline uint_fast8_t ByteOrderDataStream::GetTypeMask<int64_t >() const { return m_maskInt64; }
template<> inline uint_fast8_t ByteOrderDataStream::GetTypeMask<uint32_t>() const { return m_maskInt32; }
template<> inline uint_fast8_t ByteOrderDataStream::GetTypeMask<int32_t >() const { return m_maskInt32; }
template<> inline uint_fast8_t ByteOrderDataStream::GetTypeMask<uint16_t>() const { return m_maskInt16; }
template<> inline uint_fast8_t ByteOrderDataStream::GetTypeMask<int16_t >() const { return m_maskInt16; }
template<> inline uint_fast8_t ByteOrderDataStream::GetTypeMask<double  >() const { return m_maskDouble; }
template<> inline uint_fast8_t ByteOrderDataStream::GetTypeMask<float   >() const { return m_maskFloat; }

template<>
inline void ByteOrderDataStreamReader::read<8>(uint8_t* data,const uint8_t* buffer,uint_fast8_t mask) const{
    data[mask ^ 0]= *buffer ++;
    data[mask ^ 1]= *buffer ++;
    data[mask ^ 2]= *buffer ++;
    data[mask ^ 3]= *buffer ++;
    data[mask ^ 4]= *buffer ++;
    data[mask ^ 5]= *buffer ++;
    data[mask ^ 6]= *buffer ++;
    data[mask ^ 7]= *buffer ++;
}

template<>
inline void ByteOrderDataStreamReader::read<4>(uint8_t* data,const uint8_t* buffer,uint_fast8_t mask) const{
    data[mask ^ 0]= *buffer ++;
    data[mask ^ 1]= *buffer ++;
    data[mask ^ 2]= *buffer ++;
    data[mask ^ 3]= *buffer ++;
}
template<>
inline void ByteOrderDataStreamReader::read<2>(uint8_t* data,const uint8_t* buffer,uint_fast8_t mask) const{
    data[mask ^ 0]= *buffer ++;
    data[mask ^ 1]= *buffer ++;
}
template<>
inline void ByteOrderDataStreamReader::read<1>(uint8_t* data,const uint8_t* buffer,uint_fast8_t mask) const{
    data[mask ^ 0]= *buffer ++;
}

template<>
inline void ByteOrderDataStreamWriter::write<8>(const uint8_t* data,uint8_t* buffer,uint_fast8_t mask) const{
    *buffer ++ = data[mask ^ 0];
    *buffer ++ = data[mask ^ 1];
    *buffer ++ = data[mask ^ 2];
    *buffer ++ = data[mask ^ 3];
    *buffer ++ = data[mask ^ 4];
    *buffer ++ = data[mask ^ 5];
    *buffer ++ = data[mask ^ 6];
    *buffer ++ = data[mask ^ 7];
}
template<>
inline void ByteOrderDataStreamWriter::write<4>(const uint8_t* data,uint8_t* buffer,uint_fast8_t mask) const{
    *buffer ++ = data[mask ^ 0];
    *buffer ++ = data[mask ^ 1];
    *buffer ++ = data[mask ^ 2];
    *buffer ++ = data[mask ^ 3];
}
template<>
inline void ByteOrderDataStreamWriter::write<2>(const uint8_t* data,uint8_t* buffer,uint_fast8_t mask) const{
    *buffer ++ = data[mask ^ 0];
    *buffer ++ = data[mask ^ 1];
}
template<>
inline void ByteOrderDataStreamWriter::write<1>(const uint8_t* data,uint8_t* buffer,uint_fast8_t mask) const{
    *buffer ++ = data[mask ^ 0];
}

template<>
inline ByteOrderDataStreamReader& ByteOrderDataStreamReader::operator >> (std::string &data)
{
    ReadPascalString(data);
    return *this;
}
template<>
inline ByteOrderDataStreamWriter& ByteOrderDataStreamWriter::operator << (const std::string &data)
{
    WritePascalString(data);
    return *this;
}

template<typename T>
void ByteOrderDataStreamWriter::WriteToOffset(const T &data, ptrdiff_t writeOffset)
{
    write<sizeof(T)>(reinterpret_cast<const uint8_t*>(&data), m_buf->begin() + writeOffset, this->GetTypeMask<T>());
}

}
