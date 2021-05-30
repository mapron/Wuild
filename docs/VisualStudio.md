# Visual Studio integration
Starting from VS 2017, I highly recommend using its "Open Folder" feature wich configures project from CMakeLists.txt using ninja generator.  
You need to replace Ninja path with WuildNinja and that's it.  
  
Another solution is to use msbuild2ninja tool (https://github.com/mapron/msbuild2ninja) after you configure you project for MSBuild generator.  
call it like that:  
```
"Msbuild2ninja.exe" --build "/cmake/build-directory" --ninja "/absolute/path/bin/WuildNinja.exe" --cmake "C:/Program Files/CMake/bin/cmake.exe" --preferred-config "Release"
```
and you will get your solution converted to one that uses Makefile type (and NMake tool being WuildNinja).  
Known limitations:  
- CMake custom targets with no DEPENDS work poorly;
- You probably can get some trouble if Debug and Release custom steps output same file;
- It does not handle every possible build option;  
- It probably won't work well for any project that is not CMake-generated.

So be aware not to use it for any production quality build.
