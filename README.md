# About Wuild
Wuild (derived from "wild build") is a distributed compilation system, inspired by Distcc project. Main goals:
- Cross-platform builds (for example, Linux guests and Windows hosts);
- Simplicity;
- Fast integration and usability.

Wuild is written in C++, using Ninja (<https://github.com/ninja-build>) as main frontend. WuildNinja aims to be drop-in replacement for ninja, without need to change your project.  

# Wuild features
What's Wuild good for?  
- It provides drop-in support for ninja tool - for the best case it's just one line in your build scripts with CMake parameter;
- It supports fast incremental build same way ninja does - you don't need to switch to WuildNinja when you need to build whole project and back to ninja when you need to compile 2 files;
- It supports distributed builds and crosscompilation for clang, msvc and gcc (gcc is not highly tested though);
- Crosscompilation support means you can use same server for msvc and clang, for example;
- Support for macOS, Windows and Linux hosts;
- It does not require installation on clients - you can place WuilNinja and Wuild.ini in conan package and automatically provide setup for clients;
- Main approach is to replace build system, not the compiler binary - so you don't need to provide -j 100 to your build system and then die in linker phase.  
  
So, as you can see why you can use it as distcc replacement; it more like Incredibuild approach overall.  
It has drawbacks too:  
- It does not have any auto installer or auto configuration - you need to setup your servers by hand and configure each compiler in ini file;  
- It does not have fancy IDE integration - you almost need to have ninja support in first place. Wuild provides two workarounds you can use for MSVC/XCode, you can read about them in "Advanced usage";
- Meson support is possible and may be working, but is not tested now - main focus is on CMake ninja generator;  
- No gui tools for configuring or diagnostics - however it has plenty of commandline tools;  
- You need to manually ensure you have equal version of tools at server and client (though Wuild provides error message for diverge and ability to supress it);
If you ok with drawbacks (I am trying to reveal everything here), then project will fit you well.

# Code structure and terminology
Platform directory contains platform core primitives to base on: sockets, threads, file utilities.  
CompilerInterface declares CompilerInvocation and LocalExecutorTask.  
Configs contains settings structures for other services.  
ConfiguredApplication integrates utility options.  
Main application logic contained in Modules directory:
- LocalExecutor is lib for running tasks on local host based on Ninja's SubprocessSet;  
- InvocationRewriter splits tool invocation into preprocess and compilation; 
- RemoteToolServer executes compilator on a host, and recives tasks for such execution;
- CoordinatorSever maintains list of RemoteToolServers;
- RemoteToolClient sends compilation task to different ToolServers; ToolServers retrived from Coordinator;
- CompilerProxyServer uses RemoteToolClient to send requests; it handles local tcp connection to recieve compiler invocations;
- CompilerProxyClient acts as usual tool frontend; it sends invocation to ProxyServer, then outputs response to std out.
Main application binaries holded in root directory.

# Building from source
### Requirements 
- CMake 3.5+ (3.12 and 3.20 are tested, earlier version should be fine, file a bug if not)
- C++ compiler with either C+\+17 support or C+\+14 (with USE_GHC_STL CMake option). MSVC 2019, GCC 10 and Clang 11 are tested; previous versions also should work( GCC 6, MSVC 2015)
- System zlib is recommeded, but you can pass USE_SYSTEM_ZLIB=false to build it from source. For MSVC this is a default.

### Building
Just use usual cmake build/install steps, example for platform with Ninja generator:  
```
mkdir build && cd build
cmake -G "Ninja" -DCMAKE_INSTALL_PREFIX=install ..
cmake --build . --target install
```
Now all binaries are located in your build/install/bin folder.  
You could also just use any IDE with CMake support to build binaries.  
Packaging systems integration is not supported yet.  

# Quick start
There are four short steps, and after each, you have a state you can check properly.  
We will assume for now we have two Linux machines with g++ on them, 192.168.0.1 will be a client (where we want to speedup the build) and 192.168.0.2 will be a server (it will provide some CPU time).  
For clang or modern MSVC 2019 there is not much of a difference in setup steps if you using equal environment for both hosts.  

### Replacing ninja with WuildNinja
Modify your CMake configure command to pass CMAKE_MAKE_PROGRAM parameter, like this:  
```
cmake -G "Ninja" -DCMAKE_MAKE_PROGRAM=/path/to/WuildNinja <..>
```  
Or you can edit CMAKE_MAKE_PROGRAM cache variable in IDE as well.  
After this step nothing should break at all - if you already using recent Ninja version. Otherwise, you can get some manifest errors, like duplicate output rules (which was the warning before).  

### Configuring tools for the client host
Now we will provide info for client so it can split compilation phase into preprocessing and compilation.  
We need to create ~/.Wuild/Wuild.ini file:
```
[tools]
toolIds=gcc_cpp       ; comma-separated logical compiler names - they are used as an interaction key for client-server communication.
gcc_cpp=/usr/bin/g++  ; comma-separated possible binaries names, first must be absolute. You should start with the same absolute path you have in your CMAKE_CXX_COMPILER variable.

[toolClient]
coordinatorHost=localhost    ;  for now it does nothing but tells we have not-empty setup so we can preprocess things.
coordinatorPort=7767         ;  any reasonable port here, at this point that does not matter
```

After this setup, run again build over you project with WuildNinja. Now you should see in a build log output with "Preprocessing" prefix, like that:  
```
[1/300] Preprocessing CMakeFiles/buildTest.dir/pp_main.cpp
[2/300] Building CXX object CMakeFiles/buildTest.dir/main.cpp.o
```
Also, number of steps in square brackets should be now almost doubled (for the case your build is mostly a compilation).  
If not, check the warining output in the top of build log, you can probably find a message like that:  
```
Wuild is configured for these compiler paths:
/usr/bin/g++
but none of them used when trying to match Ninja config for the rules:
/usr/local/bin/g++
```
Then check again what compiler path you configuring CMake and adjust the path in config.  
If again, you could not see a "Preprocessing" stage, you probably need to file a bug, because Wuild fails to parse your compiler command line and detect any valid invocation. If it works for compilation of Wuild sources itself, then it definitely a bug.  
Another memorable issue on this stage - you now can get a compilation error, which was not happen before. That is possible for rare occasion when compiler give different result in PP mode, for example, MSVC sometimes fails to preprocess some raw literals if they have a "comment" inside.  
```
auto foo = R"raw(something//like_that)raw";  // works in compilation, but in preprocess everything after inner // is removed.
``` 
The only known workaround is to fix raw string literals, for example, splitting them into two.

### Configuring toolserver and coordinator on client machine
Now we will extend client config, so we can run three required tools for client-server setup: WuildNinja, WuildToolserver (it provides a remote compilation), WuildCoordinator (shows info about remote tools to clients).  
It looks that it is excess to have a coordinator when you have only one remote host, but if you want a distributed build, you probably want more than one (otherwise it's not worth enough).  
It is a bit tedious for "quick start" setup, but after that - you basically have everything you want.  
Modify your ~/.Wuild/Wuild.ini file:  
```
[tools]
toolIds=gcc_cpp
gcc_cpp=/usr/bin/g++

[toolClient]
coordinatorHost=localhost
coordinatorPort=7767         ; any port your firewall is happy with
minimalRemoteTasks=0         ; that's not required, it's default 10, if you testing with a fair small project, Wuild just won't trigger a remote build at all. you need to remove this later if you need a fast incremented builds. 

[toolServer]
listenPort=7765              ; any port diferent from coordinator's
listenHost=localhost
coordinatorHost=localhost
coordinatorPort=7767         ; exact same as in [toolClient]
threadCount=2                ; for now it's not important, you should not to high for localhost tests, as it can slowdown the compilation if you get above you logical cores.

[coordinator]
listenPort=7767              ; exact same as [toolClient] and [toolServer] coordinatorPort
```
Before you run build you project again, you need 2 separate terminal windows. Change dir to your Wuild install, there should be 2 binaries called WuildCoordinator and WuildToolServer, execute both:
```
./WuildCoordinator
```
and (second terminal)
```
./WuildToolServer
```

Now run again build of your project (don't forget to clean previous build), you should see now ```[REMOTE]``` marked steps in the build output (and 2 more lines of output ine the begin and the end):  

```
Recieved info from coordinator: total remote threads=2, free=2
[1/3] Preprocessing CMakeFiles/buildTest.dir/pp_main.cpp
[2/3] [REMOTE] Building CXX object CMakeFiles/buildTest.dir/main.cpp.o
[3/3] Linking CXX executable buildTest
sid:1622371618148204(err:0), maxThreads:0 Total remote tasks:1, done in 00:00 total compilationTime: 293174 us.,  total networkTime: 568258 us.,  total overhead: 93% sent KiB: 173,  recieved KiB: 4,  compression time: 38826 us., 
```
Also, you should notice some output in WuildToolServer's console:  
```
gcc_cpp: <someflags> -c <sometmppath>/0_pp_main.cpp -> <sometmppath>/0_main.cpp.o [12859 / 4343]
```
If that is not happening, try to run WuildCoordinatorStatus and WuildToolServerStatus tools while WuildCoordinator and WuildToolServer are active. You should see some output with tool ids and versions, if ini is configured correctly.  
Next step, you can add debug logging, adding ```logLevel=6``` or even ```logLevel=7``` line to Wuild.ini or passing it as argument ```--wuild-logLevel=6``` to all of the tools.  
If this still not helping at all, and you do not see ```[REMOTE]``` in build output, please, file a bug with all logs and ini attached.  

### Splitting execution between two hosts
This is the final and probably the easiest part (if you don't have complicated firewall rules in your network).  
Now prepare all Wuild binaries on the second host, we will use "192.168.0.2" for it, but you can use a hostname as well, if it is resolvable.  
Start with modifying you client's ini:  
```
[tools]
toolIds=gcc_cpp
gcc_cpp=/usr/bin/g++

[toolClient]
coordinatorHost=192.168.0.2
coordinatorPort=7767         ; any port your firewall is happy with
```
And create ~/.Wuild/Wuild.ini on the server:
```
[tools]
toolIds=gcc_cpp        ; server need tools to execute a compilation step. However, it does not require any includes, or libraries, as it only does compilation steps of fully preprocessed TU.
gcc_cpp=/usr/bin/g++   ; it can have a different path, that does not matter, just id should match.

; don't need a [toolClient] section

[toolServer]
listenPort=7765              ; any port diferent from coordinator's
listenHost=192.168.0.2       ; IMPORTANT! we can't just write 'localhost' here, as this exact host name will be send to slient. If you use a string hostname, make sure it's resolvable on client side.
coordinatorHost=localhost
coordinatorPort=7767         ; exact same as in [toolClient]
threadCount=64               ; for the start - use logical cores available. You can tune it down later for better load.

[coordinator]
listenPort=7767              ; exact same as [toolServer] coordinatorPort. make sure to by sync with 192.168.0.1 client config.
```
Now you can stop WuildCoordinator and WuildToolServer on the client, and run both on the server (192.168.0.2) machine. Now you will have build distributed between two hosts.

# Configuration: commandline and config file
For every option in ini file, you can also provide its value form the commandline; you need to add ```--wuild-``` prefix to it, for example:  
```
WuildNinja --wuild-logLevel=7 --wuild-coordinatorHost=testhost
```  
Any commandline option has higher priority that ini option. However, keep in mind, that all commandline variable placed in 'global' scope, so you can't provide different 'listenPort' for different scopes.  
  
Let's now talk about ini config file location. It tries to load these locations in order:  
- ```--wuild-config=/path/file.ini``` command line parameter (and stops with error if path provided but invalid);
- ```WUILD_CONFIG=/path/file.ini``` environment variable (and stops with error if path provided but invalid);
- ```~/.Wuild/Wuild.ini``` (for Windows ~ is %USERPROFILE%);
- (Windows only) ```~/Wuild.ini```;
- Final attempt, it will try to load ```Wuild.ini``` in the same directory with main executable (e.g. WuildNinja).  
  
If first two was not set and search is ended, then Wuild is set to "unconfigured mode". For WuildNinja it means it will fallback to plain ninja.

# Advanced usage
After Quick Start, next step is to add more ToolServers and maybe Coordinators (so you can safely shutown one server without disabling all builds on CI).  
You don't need to have Coordinator on the same host as ToolServer (and you can host Coordinators on lightweight VMs as they do not require much of resource).  
Also, you need to create autolaunch routing for ToolServers and Coordinators  (you may be interested in NSSM tool for Windows).  
Next what you probably iterested in - you can use Clang as crosscompiler, for example, MacOS clients and Windows toolservers, you can [read about setup here](docs/CrossCompilation.md).  
Another case is integrate with builds, where ninja is not available, for example, Make or xcodebuild systems, you can read more [here about WuildProxy](docs/WuildProxy.md).  
For troubleshooting and debugging you builds, you can read [Troubleshooting doc page](docs/Troubleshooting.md).  
For "how to run in Visual Studio", you can read about [VS integration](docs/VisualStudio.md).  
If you want optimize you compilation speed to limits and do becnhmarking, some advice can be found in [Performance article](docs/Performance.md).  
And finally, we have ini config [option reference](docs/ExampleConfigurations/Wuild-full-options.ini).  

# Known issues
- Precompiled headers are unsupported;
- /Zi (PDB) for MSVC is unsupported, but workarounded switching to Z7 (it may be unexpected for someone); it also can break some builds if too large static libraries linking (use dlls).
- serialize-diagnostics option is ignored;
- ProxyServer tool server connection kept forever. 

# Road map
- Installer and packaging support;
- Wildcard or another approach to support servers with dozens of tools;
- C++ modules support (when they are stable in CMake/ninja/clang/msvc at least).

# Used thirdparty products
- (Apache 2) "Ninja build system" (https://github.com/ninja-build) is used for a fast frontentd;
- (BSD) ZStd library used for compression algorithms;
- (MIT) Hlohmann Json library (https://github.com/nlohmann/json) is used for json output in debug tools;
- (MIT) GHC filesystem library (https://github.com/gulrak/filesystem) for a C++17-compatible Filesystem implementation.

# Credits
- Special thanks to Peter Zhigalov (https://github.com/AlienCowEatCake) for the thorough alpha- and beta-testing;
- Thanks for Sergio Torres Soldado for constructive feedback;
- Thanks to Movavi (https://www.movavi.com) team for the testing environment.

# Licensing 
  Copyright (C) 2017-2021 Smirnov Vladimir mapron1@gmail.com  
  Source code licensed under the Apache License, Version 2.0 (the "License");  
  You may not use this file except in compliance with the License.  
  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 or in [file](COPYING-APACHE-2.0.txt)  

  Unless required by applicable law or agreed to in writing, software  
  distributed under the License is distributed on an "AS IS" BASIS,  
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  
  See the License for the specific language governing permissions and  
  limitations under the License.h  
