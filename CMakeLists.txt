#[[
  Copyright (C) 2017-2024 Smirnov Vladimir mapron1@gmail.com
  Source code licensed under the Apache License, Version 2.0 (the "License");
  You may not use this file except in compliance with the License.
  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 or in file COPYING-APACHE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.h
#]]

cmake_minimum_required(VERSION 3.24)
project(Wuild)

# main paths.
set(NINJA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ninja/src)
set(MERNEL_ROOT ${CMAKE_CURRENT_LIST_DIR}/ext/mernel)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake ${MERNEL_ROOT}/cmake)
set(srcRoot ${CMAKE_CURRENT_LIST_DIR}/src)

#configure options
option( WARNING_AS_ERROR "" OFF )

#platform configuration.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(utils)
include(targets)

if (WIN32)
	add_definitions(-DNOMINMAX -D_UNICODE -DUNICODE -D_SCL_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES)
endif()

if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_HAS_STD_BYTE=0)
	add_compile_options(/wd4267 /wd4244 /Zc:__cplusplus)
else()
	add_compile_options(-Wall -Wno-error=sign-compare)
endif()

if ( WIN32)
	set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} ws2_32.lib")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ws2_32.lib")
else()
	set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -lpthread")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lpthread")
endif()
if (APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
endif()

if (WARNING_AS_ERROR)
	if (MSVC)
		add_compile_options(/WX) #warning as error.
	else()
		add_compile_options(-Werror)
	endif()
endif()


#[[
########## Mernel ##########
#]]
set(MERNEL_BUILD_SHARED OFF)
include(ext/mernel/mernel_thirdparty.cmake)
include(ext/mernel/mernel.cmake)


# Source code targets.
set(ninja_lib_src
	## build.cc
	build_log.cc
	clean.cc
	clparser.cc
	debug_flags.cc
	depfile_parser.cc
	deps_log.cc
	disk_interface.cc
	dyndep.cc
	dyndep_parser.cc
	parser.cc
	edit_distance.cc
	eval_env.cc
	graph.cc
	graphviz.cc
	lexer.cc
	line_printer.cc
	manifest_parser.cc
	metrics.cc
	## ninja.cc
	state.cc
	util.cc
	version.cc
	string_piece_util.cc
	)

if (WIN32)
	set(ninja_subprocess_src subprocess-win32.cc )
	list(APPEND ninja_lib_src includes_normalize-win32.cc )
	if (MSVC)
		list(APPEND ninja_lib_src minidump-win32.cc msvc_helper-win32.cc msvc_helper_main-win32.cc getopt.c)
	endif()
else()
	set(ninja_subprocess_src subprocess-posix.cc )
endif()


AddTarget(TYPE static NAME ninja_lib SOURCE_DIR ${NINJA_ROOT}/ SKIP_GLOB EXTRA_GLOB ${ninja_lib_src} ${ninja_subprocess_src} *.h)
if (MSVC)
	target_compile_options(ninja_lib PRIVATE /wd4091 /wd4800 /wd4996)
endif()

AddTarget(TYPE static NAME Platform SOURCE_DIR ${srcRoot}/Platform EXPORT_INCLUDES
	LINK_LIBRARIES MernelPlatform
	)

AddTarget(TYPE static NAME Configs SOURCE_DIR ${srcRoot}/Configs EXPORT_INCLUDES
	LINK_LIBRARIES Platform MernelPlatform)

AddTarget(TYPE static NAME ToolExecutionInterface SOURCE_DIR ${srcRoot}/ToolExecutionInterface EXPORT_INCLUDES
	LINK_LIBRARIES Configs Platform MernelPlatform)

AddTarget(TYPE static NAME Coordinator SOURCE_DIR ${srcRoot}/Modules/Coordinator EXPORT_INCLUDES
	LINK_LIBRARIES Configs Platform MernelPlatform ToolExecutionInterface
	)

AddTarget(TYPE static NAME RemoteTool SOURCE_DIR ${srcRoot}/Modules/RemoteTool EXPORT_INCLUDES
	LINK_LIBRARIES Configs Platform MernelPlatform ToolExecutionInterface Coordinator
	)

AddTarget(TYPE static NAME ToolProxy SOURCE_DIR ${srcRoot}/Modules/ToolProxy EXPORT_INCLUDES
	LINK_LIBRARIES Configs Platform MernelPlatform ToolExecutionInterface RemoteTool Coordinator
	)

AddTarget(TYPE static NAME InvocationTool SOURCE_DIR ${srcRoot}/Modules/InvocationTool EXPORT_INCLUDES
	LINK_LIBRARIES Configs Platform MernelPlatform ToolExecutionInterface
	)
AddTarget(TYPE static NAME VersionChecker SOURCE_DIR ${srcRoot}/Modules/VersionChecker EXPORT_INCLUDES
	LINK_LIBRARIES Configs Platform MernelPlatform ToolExecutionInterface
	)
AddTarget(TYPE static NAME LocalExecutor SOURCE_DIR ${srcRoot}/Modules/LocalExecutor EXPORT_INCLUDES
	LINK_LIBRARIES ninja_lib Configs Platform MernelPlatform InvocationTool ToolExecutionInterface
	INCLUDES ${NINJA_ROOT}/
	)

AddTarget(TYPE static NAME ConfiguredApplication SOURCE_DIR ${srcRoot}/ConfiguredApplication EXPORT_INCLUDES
	LINK_LIBRARIES Configs Platform MernelPlatform
	)

set(main_deps
	ConfiguredApplication Configs VersionChecker LocalExecutor InvocationTool ToolExecutionInterface ToolProxy RemoteTool Coordinator Platform MernelPlatform
	)

AddTarget(TYPE static NAME TestUtil SOURCE_DIR ${srcRoot}/TestUtil/ EXPORT_INCLUDES
	LINK_LIBRARIES ${main_deps}
	)

foreach (testname AllConfigs Balancer Compiler Coordinator Inflate Networking ToolServer CommandLine)
	AddTarget(TYPE app_console NAME Test${testname} SOURCE_DIR ${srcRoot}/TestsManual
		SKIP_GLOB EXTRA_GLOB Test${testname}.cpp
		LINK_LIBRARIES ${main_deps} TestUtil
		)
endforeach()
foreach (benchname NetworkClient NetworkServer)
	AddTarget(TYPE app_console NAME Benchmark${benchname} SOURCE_DIR ${srcRoot}/Benchmarks
		SKIP_GLOB EXTRA_GLOB Benchmark${benchname}.cpp *.h BenchmarkUtils.cpp
		LINK_LIBRARIES ${main_deps}
		)
endforeach()

foreach (appname Coordinator CoordinatorStatus Proxy ProxyClient ToolExecutor ToolServer ToolServerStatus)
	AddTarget(TYPE app_console NAME Wuild${appname} SOURCE_DIR ${srcRoot}/Apps/${appname}/
		LINK_LIBRARIES ${main_deps}
		)
	install(TARGETS Wuild${appname} RUNTIME DESTINATION bin)
endforeach()

AddTarget(TYPE app_console NAME WuildNinja SOURCE_DIR ${srcRoot}/Apps/Ninja/
	EXTRA_SOURCES ${NINJA_ROOT}/ninja.cc ${NINJA_ROOT}/build.cc
	LINK_LIBRARIES ${main_deps}
	INCLUDES ${srcRoot}/Apps/Ninja/ ${NINJA_ROOT}/
	)
install(TARGETS WuildNinja RUNTIME DESTINATION bin)
