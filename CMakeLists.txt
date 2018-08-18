#[[
  Copyright (C) 2017 Smirnov Vladimir mapron1@gmail.com
  Source code licensed under the Apache License, Version 2.0 (the "License");
  You may not use this file except in compliance with the License.
  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 or in file COPYING-APACHE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.h
#]]

cmake_minimum_required(VERSION 3.5)
project(Wuild)

# main paths.
set(NINJA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ninja-1.8.2/src)
set(ZLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zlib)
set(LZ4_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/lz4-1.7.3)
set(LZ4_STREAM_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/lz4_stream)
set(ZSTD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zstd/lib)

#configure options
option( WARNING_AS_ERROR "" ON )
option( USE_ZLIB_COMPRESSION "" ON )
option( USE_LZ4_COMPRESSION "" ON )
option( USE_ZSTD_COMPRESSION "" ON )
option( USE_SYSTEM_ZLIB "Use system zlib library." ON )
SET( BOOST_INCLUDEDIR "" CACHE PATH "Boost path to includes")
SET( BOOST_LIBRARYDIR "" CACHE PATH "Boost path to compiled libs")

#platform configuration.
if (MSVC)
	set(USE_SYSTEM_ZLIB OFF)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX /wd4267 /wd4244)
endif()

set(HAS_BOOST)
if (BOOST_LIBRARYDIR)
   find_package(Boost REQUIRED system filesystem)
   set(HAS_BOOST -DHAS_BOOST=1)
endif()

if (NOT MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z ")
endif()

set(sys_deps)

if ( WIN32)
	list(APPEND sys_deps ws2_32.lib)
else()
	list(APPEND sys_deps -lpthread)
endif()

if (WARNING_AS_ERROR)
    if (MSVC)
        add_compile_options(/WX) #warning as error.
    else()
        add_compile_options(-Werror -Wall -Wno-error=sign-compare)
    endif()
endif()

if (APPLE)
	list(APPEND sys_deps -stdlib=libc++ -lc++)
	add_definitions(-D_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR)
endif()

if (NOT BOOST_LIBRARYDIR)
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		list(APPEND sys_deps  -lc++experimental)  # Libc++experimental.a may not always be available, even when libc++ is already installed
	else()
		if (NOT MSVC)
			list(APPEND sys_deps  -lstdc++fs)
		endif()
	endif()
endif()

# function for target declaration.
function(AddTarget)
	set(options APP)
	set(oneValueArgs NAME ROOT SUBDIR)
	set(multiValueArgs CSRC INCLUDES DEPS EXCLUDE OPTIONS DEFINES)
	cmake_parse_arguments(AddTarget "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
	set(_sources)
	if (NOT AddTarget_ROOT)
		set(AddTarget_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/${AddTarget_SUBDIR}${AddTarget_NAME})
	endif()
	foreach (csrc ${AddTarget_CSRC})
		file(GLOB src ${AddTarget_ROOT}/${csrc})
		list(APPEND _sources ${src})
	endforeach()
	foreach (excl ${AddTarget_EXCLUDE})
		list(REMOVE_ITEM _sources ${AddTarget_ROOT}/${excl})
	endforeach()
	if (AddTarget_APP)
		add_executable(${AddTarget_NAME} ${_sources})
	else()
		add_library(${AddTarget_NAME} STATIC ${_sources})
	endif()
	foreach (inc ${AddTarget_INCLUDES})
		target_include_directories(${AddTarget_NAME} PRIVATE ${inc})
	endforeach()
	target_include_directories(${AddTarget_NAME} INTERFACE ${AddTarget_ROOT})
	foreach (dep ${AddTarget_DEPS})
		target_link_libraries(${AddTarget_NAME} PRIVATE ${dep})
	endforeach()
	foreach (opt ${AddTarget_OPTIONS})
		target_compile_options(${AddTarget_NAME} PRIVATE ${opt})
	endforeach()
	foreach (opt ${AddTarget_DEFINES})
		target_compile_definitions(${AddTarget_NAME} PRIVATE ${opt})
	endforeach()
endfunction()

# Compression targets
if (USE_ZLIB_COMPRESSION)
	if (USE_SYSTEM_ZLIB)
		list(APPEND COMPRESSION_LIBS -lz)
	else()
		AddTarget(NAME zlib_static ROOT ${ZLIB_ROOT}/ CSRC *.c)
		list(APPEND COMPRESSION_LIBS zlib_static)
		target_compile_options(zlib_static PRIVATE  /wd4996)
	endif()
	list(APPEND COMPRESSION_DEFINES -DUSE_ZLIB)
endif()
if (USE_LZ4_COMPRESSION)
	AddTarget(NAME lz4_static ROOT ${LZ4_ROOT}/ CSRC *.c)
	AddTarget(NAME lz4_stream ROOT ${LZ4_STREAM_ROOT}/ CSRC *.cpp DEPS lz4_static)
	list(APPEND COMPRESSION_LIBS lz4_stream lz4_static)
	list(APPEND COMPRESSION_DEFINES -DUSE_LZ4)
endif()

if (USE_ZSTD_COMPRESSION)
	AddTarget(NAME zstd_static ROOT ${ZSTD_ROOT}/ CSRC common/*.c compress/*.c  decompress/*.c
		INCLUDES ${ZSTD_ROOT}/ ${ZSTD_ROOT}/common/
		)
	list(APPEND COMPRESSION_LIBS zstd_static)
	list(APPEND COMPRESSION_DEFINES -DUSE_ZSTD)
endif()

# Source code targets.
set(ninja_lib_src
  #  browse.cc
	build.cc
	build_log.cc
	clean.cc
	clparser.cc
	debug_flags.cc
	depfile_parser.cc
	deps_log.cc
	disk_interface.cc
	edit_distance.cc
	eval_env.cc
	graph.cc
	graphviz.cc
	lexer.cc
	line_printer.cc
	manifest_parser.cc
	metrics.cc
	state.cc
  #  subprocess-posix.cc
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

AddTarget(NAME ninja_subprocess ROOT ${NINJA_ROOT}/ CSRC ${ninja_subprocess_src}
	)
AddTarget(NAME ninja_lib ROOT ${NINJA_ROOT}/ CSRC ${ninja_lib_src} *.h
	EXCLUDE remote_executor_impl.h
	DEPS ninja_subprocess
	)
if (MSVC)
	target_compile_options(ninja_lib PRIVATE /wd4091 /wd4800 /wd4996)
endif()

AddTarget(NAME Platform CSRC *.cpp *.h
	INCLUDES
	${BOOST_INCLUDEDIR}
	DEFINES
	 ${HAS_BOOST} ${COMPRESSION_DEFINES}
	DEPS
	${COMPRESSION_LIBS}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_SYSTEM_LIBRARY}
	)

AddTarget(NAME Configs CSRC *.cpp *.h
	DEPS Platform)

AddTarget(NAME ToolExecutionInterface CSRC *.cpp *.h
	DEPS Configs Platform)

AddTarget(NAME Coordinator SUBDIR Modules/ CSRC *.cpp *.h
	DEPS Configs Platform ToolExecutionInterface
	)

AddTarget(NAME RemoteTool SUBDIR Modules/ CSRC *.cpp *.h
	DEPS Configs Platform ToolExecutionInterface Coordinator
	)

AddTarget(NAME ToolProxy SUBDIR Modules/ CSRC *.cpp *.h
	DEPS Configs Platform ToolExecutionInterface RemoteTool Coordinator
	)

AddTarget(NAME InvocationRewriter SUBDIR Modules/ CSRC *.cpp *.h
	DEPS Configs Platform ToolExecutionInterface
	)
AddTarget(NAME VersionChecker SUBDIR Modules/ CSRC *.cpp *.h
	DEPS Configs Platform ToolExecutionInterface
	)
AddTarget(NAME LocalExecutor SUBDIR Modules/  CSRC *.cpp *.h
	DEPS ninja_subprocess Configs Platform InvocationRewriter ToolExecutionInterface
	)

AddTarget(NAME ConfiguredApplication CSRC *.cpp *.h
	DEPS Configs Platform
	)

set(main_deps
	ConfiguredApplication Configs VersionChecker LocalExecutor InvocationRewriter ToolExecutionInterface ToolProxy RemoteTool Coordinator Platform ninja_subprocess ninja_lib
	)

foreach (testname AllConfigs Balancer Compiler Coordinator Inflate Networking ToolServer )
	AddTarget(APP NAME Test${testname} ROOT ${CMAKE_CURRENT_SOURCE_DIR}/TestsManual/
		CSRC Test${testname}.cpp *.h TestUtils.cpp
		DEPS ${main_deps} ${sys_deps}
		)
endforeach()

foreach (appname Coordinator CoordinatorStatus ToolServer Proxy ProxyClient ToolExecutor)
	AddTarget(APP NAME Wuild${appname} ROOT ${CMAKE_CURRENT_SOURCE_DIR}/
		CSRC Wuild${appname}.cpp
		DEPS ${main_deps} ${sys_deps}
		)
endforeach()

AddTarget(APP NAME WuildNinja ROOT ${NINJA_ROOT}/
	CSRC ninja.cc remote_executor_impl.* state_rewrite.*
	DEPS ${main_deps} ninja_subprocess ${sys_deps}
	)
