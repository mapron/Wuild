// Own headers
#include "lz4_stream.h"

LZ4InputStream::LZ4InputBuffer::LZ4InputBuffer(std::istream &source)
  : source_(source),
	src_buf_(),
	dest_buf_(),
	offset_(0),
	src_buf_size_(0)
{
  size_t ret = LZ4F_createDecompressionContext(&ctx_, LZ4F_VERSION);
  if (LZ4F_isError(ret))
  {
	throw std::runtime_error(std::string("Failed to create LZ4 decompression context: ")
							 + LZ4F_getErrorName(ret));
  }
  setg(&src_buf_.front(), &src_buf_.front(), &src_buf_.front());
}

LZ4InputStream::int_type LZ4InputStream::LZ4InputBuffer::underflow()
{
	size_t dest_size=0;
	size_t ret =0;
	do
	{
		if (offset_ == src_buf_size_)
		{
			source_.read(&src_buf_.front(), src_buf_.size());
			src_buf_size_ = static_cast<size_t>(source_.gcount());
			offset_ = 0;
		}

		if (src_buf_size_ == 0)
		{
			return traits_type::eof();
		}

		size_t src_size = src_buf_size_ - offset_;
		dest_size = dest_buf_.size();
		ret = LZ4F_decompress(ctx_, &dest_buf_.front(), &dest_size,
									 &src_buf_.front() + offset_, &src_size, NULL);
		offset_ += src_size;
		if (LZ4F_isError(ret))
		{
			throw std::runtime_error(std::string("LZ4 decompression failed: ")
									 + LZ4F_getErrorName(ret));
		}
		if (dest_size > 0)
		{
			break;
		}
		if (ret == 0)
		{
			return traits_type::eof();
		}

		if (ret > 0)
		{
			continue;
		}
		break;
	}while(true);

	setg(&dest_buf_.front(), &dest_buf_.front(), &dest_buf_.front() + dest_size);

  return traits_type::to_int_type(*gptr());
}

LZ4InputStream::LZ4InputBuffer::~LZ4InputBuffer()
{
  LZ4F_freeDecompressionContext(ctx_);
}
