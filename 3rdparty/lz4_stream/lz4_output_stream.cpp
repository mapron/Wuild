// Own headers
#include "lz4_stream.h"

// Standard headers
#include <cassert>
#include <exception>
#include <functional>

LZ4OutputStream::LZ4OutputBuffer::LZ4OutputBuffer(std::ostream &sink)
  : sink_(sink),
    // TODO: No need to recalculate the dest_buf_ size on each construction
    dest_buf_(LZ4F_compressBound(src_buf_.size(), NULL)),
    closed_(false)
{
  char* base = &src_buf_.front();
  setp(base, base + src_buf_.size() - 1);

  size_t ret = LZ4F_createCompressionContext(&ctx_, LZ4F_VERSION);
  if (LZ4F_isError(ret))
  {
    throw std::runtime_error(std::string("Failed to create LZ4 compression context: ")
                             + LZ4F_getErrorName(ret));
  }
  writeHeader();
}

LZ4OutputStream::LZ4OutputBuffer::~LZ4OutputBuffer()
{
  close();
}

LZ4OutputStream::int_type LZ4OutputStream::LZ4OutputBuffer::overflow(int_type ch)
{
  assert(std::less_equal<char*>()(pptr(), epptr()));

  *pptr() = static_cast<LZ4OutputStream::char_type>(ch);
  pbump(1);

  compressAndWrite();

  return ch;
}

LZ4OutputStream::int_type LZ4OutputStream::LZ4OutputBuffer::sync()
{
  compressAndWrite();
  return 0;
}

void LZ4OutputStream::LZ4OutputBuffer::compressAndWrite()
{
  assert(!closed_);
  std::ptrdiff_t orig_size = pptr() - pbase();
  pbump(-orig_size);
  size_t comp_size = LZ4F_compressUpdate(ctx_, &dest_buf_.front(), dest_buf_.size(),
                                         pbase(), orig_size, NULL);
  sink_.write(&dest_buf_.front(), comp_size);
}

void LZ4OutputStream::LZ4OutputBuffer::writeHeader()
{
  assert(!closed_);
  size_t ret = LZ4F_compressBegin(ctx_, &dest_buf_.front(), dest_buf_.size(), NULL);
  if (LZ4F_isError(ret))
  {
    throw std::runtime_error(std::string("Failed to start LZ4 compression: ")
                             + LZ4F_getErrorName(ret));
  }
  sink_.write(&dest_buf_.front(), ret);
}

void LZ4OutputStream::LZ4OutputBuffer::writeFooter()
{
  assert(!closed_);
  size_t ret = LZ4F_compressEnd(ctx_, &dest_buf_.front(), dest_buf_.size(), NULL);
  if (LZ4F_isError(ret))
  {
    throw std::runtime_error(std::string("Failed to end LZ4 compression: ")
                             + LZ4F_getErrorName(ret));
  }
  sink_.write(&dest_buf_.front(), ret);
}

void LZ4OutputStream::LZ4OutputBuffer::close()
{
  if (closed_)
  {
    return;
  }
  sync();
  writeFooter();
  LZ4F_freeCompressionContext(ctx_);
  closed_ = true;
}
