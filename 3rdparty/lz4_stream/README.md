lz4_stream - A C++ stream using LZ4 (de)compression
===================================================

lz4_stream is a simple wrapper that uses C++ streams for compressing and decompressing data using the [LZ4 compression library]

Usage
-----

Look at lz4\_compress.cpp and lz4\_decompress.cpp for example command line programs that can compress and decompress using this stream library.

To build the library, example programs and test code make sure [SCons] is installed and run:

    $ scons

from the command line.

Requirements
------------

The [LZ4 compression library] is required to use this library.

A SConstruct file is provided for building the code requiring [SCons] to be installed.

To build and run the tests, [Google Test Framework] should be installed.

Build status
------------
Ubuntu (GCC/Clang):

[![Build Status](https://travis-ci.org/laudrup/lz4_stream.png)](https://travis-ci.org/laudrup/lz4_stream)

Windows (MS C++):

[![Build status](https://ci.appveyor.com/api/projects/status/xrp8bjf9217broom?svg=true)](https://ci.appveyor.com/project/laudrup/lz4-stream)

License
-------

Standard BSD 3-Clause License as used by the LZ4 library.


[LZ4 compression library]: https://github.com/Cyan4973/lz4
[SCons]: http://www.scons.org
[Google Test Framework]: https://github.com/google/googletest
