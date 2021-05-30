# Running WuildProxy
Not all IDE supports Ninja. So we can integrate with make this way:
- Setup [proxy] section in Wuild.ini;
- Run WuildProxy; 
- Setup WuildProxyClient as cxx compiler, for exmaple, passing ```-DCMAKE_CXX_COMPILER=/path/to/WuildProxyClient``` to CMake;
- Run make with higher number of threads, "-j 20", or even more.

Xcode integration is preliminary; it is supported only through ProxyClient. Create symlink to WuildProxyClient instead of clang++ in Xcode toolchains, or use dispatcher script which will pass all parameters to WuildProxyClient depending on some option.
