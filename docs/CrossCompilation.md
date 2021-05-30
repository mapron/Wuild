# Cross compilation
The only tool that is highly tested for this purpose is Clang. I did not check GCC tooling yet, but it should be possible too.  
Let's imagine we have macOS client with clang 10 and Windows host with the same compiler.  
The main quirk is to add "_appendRemote=" parameter for macOS toolIds.  
Possible configuration for client is:  

```
CLANG_BIN:=/usr/local/llvm-10/bin

[tools]
toolIds=clang10_cpp,clang10_c
clang10_cpp=$CLANG_BIN/clang++
clang10_c=$CLANG_BIN/clang

;cross flags
clang10_c_appendRemote=--target=x86_64-apple-darwin15.4.0
clang10_cpp_appendRemote=--target=x86_64-apple-darwin15.4.0

; we can also force C compiler to use tool id for C++.
; clang10_c_remoteAlias=clang10_cpp

[toolClient]
coordinatorHost=192.168.0.2
coordinatorPort=7767
```
  
and for the Windows server (assuming it has Coordinator and ToolServer) is nothing special at all:  

```
; use environment variable %VS140CMNTOOLS%
MSVC_2019_VS:=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC
CLANG10:=C:\LLVM\10\bin

[tools]
toolIds=clang10_c,clang10_cpp,msvc2019_x86_64

clang10_c=$CLANG10/clang.exe
clang10_cpp=$CLANG10/clang++.exe
msvc2019_x86_64=$MSVC_2019_VS\14.29.30037\bin\Hostx64\x64\cl.exe

[toolServer]
; how many jobs will be executed concurrently.
threadCount=4
listenHost=192.168.0.2  ; client will use this hostname to connect to toolserver.
listenPort=7765
coordinatorHost=localhost
coordinatorPort=7767

[coordinator]
listenPort=7767
```