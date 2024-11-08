cmake_minimum_required(VERSION 3.13)

project(client)

include_directories(BEFORE SYSTEM include)
include_directories(BEFORE .)
link_directories(${CMAKE_SOURCE_DIR}/lib)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT(CMAKE_BUILD_TYPE OR DEFINED ENV{CMAKE_BUILD_TYPE}))
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Default build type is Debug")
  set(CMAKE_BUILD_TYPE "Debug")
  message(STATUS "Build type defaulted to ${CMAKE_BUILD_TYPE}")
else()
  message(STATUS "Build type explicitly configured as ${CMAKE_BUILD_TYPE}")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)
if(build_type STREQUAL "debug")
  message(STATUS "Build type is debug - building with disabled optimisation and debugging symbols")
  set(opt_and_debug_compiler_options
    # optimisations
    -O0
    # debugging
    -g3
  )
  set(opt_and_debug_linker_options
    # optimisations
    -O0
    # debugging
    -g3
    --profiling-funcs
    # ensure fast link in debug mode: https://emscripten.org/docs/optimizing/Optimizing-Code.html#link-times
    -sERROR_ON_WASM_CHANGES_AFTER_LINK
  )
elseif(build_type STREQUAL "release")
  message(STATUS "Build type is release - building with optimisation & minifying")
  set(opt_and_debug_compiler_options
    # optimisations
    -O3
    # re-enable once boost "result_of" errors are resolved in sysroot/lib/wasm32-emscripten/lto/libboost_headers.a
    -flto
    # debugging disabled
    -g0
  )
  set(opt_and_debug_linker_options
    # optimisations - extra options for release build: https://emscripten.org/docs/tools_reference/emcc.html
    -O3
    --no-demangle
    -flto
    --closure 1
    # currently broken:
    #-sEVAL_CTORS
    # debugging disabled
    -g0
  )
else()
  message(FATAL_ERROR "Invalid build type \"${CMAKE_BUILD_TYPE}\"")
endif()

set(EXCEPTION_HANDLING js CACHE STRING "Exception handling mode: none, js or wasm")
# enable wasm when support improves (https://emscripten.org/docs/porting/exceptions.html)
if(EXCEPTION_HANDLING STREQUAL "none")
  message(STATUS "Exception handling disabled")
  set(exception_compile_definitions
    DISABLE_EXCEPTION_THROWING
    DISABLE_EXCEPTION_CATCHING
    BOOST_NO_EXCEPTIONS
    BOOST_EXCEPTION_DISABLE
    JSON_NOEXCEPTION
  )
  set(exception_compile_options
    -fignore-exceptions
    -fno-exceptions
  )
  set(exception_link_options
    -fno-except
    -fignore-exceptions
    #-fno-exceptions
  )
elseif(EXCEPTION_HANDLING STREQUAL "js")
  message(STATUS "Exception handling using JS (slow)")
  set(exception_compile_definitions
    NO_DISABLE_EXCEPTION_CATCHING
  )
  set(exception_compile_options
    # inefficient javascript exceptions:
    -sNO_DISABLE_EXCEPTION_CATCHING
  )
  set(exception_link_options
    # inefficient javascript exceptions:
    -sNO_DISABLE_EXCEPTION_CATCHING
  )
elseif(EXCEPTION_HANDLING STREQUAL "wasm")
  message(STATUS "Exception handling using WASM)")
  set(exception_compile_definitions
    NO_DISABLE_EXCEPTION_CATCHING
  )
  set(exception_compile_options
    # enable native wasm exceptions
    -fwasm-exceptions
    -sSUPPORT_LONGJMP=wasm
  )
  set(exception_link_options
    # enable native wasm exceptions
    -fwasm-exceptions
    -sSUPPORT_LONGJMP=wasm
  )
else()
  message(FATAL_ERROR "Invalid exception handling mode \"${EXCEPTION_HANDLING}\"")
endif()

add_compile_definitions(
  BOOST_DISABLE_THREADS
  BOOST_SYSTEM_DISABLE_THREADS
  BOOST_URL_DISABLE_THREADS
  GLFW_INCLUDE_ES3
  IMGUI_IMPL_OPENGL_NO_RESTORE_STATE
  NO_BLOB_LOADER
  LOGSTORM_SINGLE_THREADED
  ${exception_compile_definitions}
)

add_executable(client
  # project-specific:
  main.cpp
  # shared libraries:
  logstorm/log_line_helper.cpp
  logstorm/manager.cpp
  logstorm/sink/base.cpp
  #logstorm/sink/circular_buffer.cpp
  logstorm/sink/console.cpp
  #logstorm/sink/console_err.cpp
  #logstorm/sink/dummy.cpp
  #logstorm/sink/file.cpp
  #logstorm/sink/fstream.cpp
  #logstorm/sink/stream.cpp
  logstorm/timestamp.cpp
  # 3rd party libraries:
  #TODO
)

target_compile_options(client PRIVATE
  ${opt_and_debug_compiler_options}
  -msse
  -msse2
  -msse3
  -mssse3
  -msse4.1
  -msse4.2
  -mavx
  -msimd128
  # emscripten ports
  -sUSE_BOOST_HEADERS=1
  -sUSE_FREETYPE=1
  ${exception_compile_options}
  # errors
  -Wfatal-errors
  # warnings
  -Wall
  -Warray-bounds
  -Wcast-align
  -Wconversion
  -Wcovered-switch-default
  -Wdisabled-optimization
  -Wdocumentation
  -Wdouble-promotion
  -Wextra
  -Wextra-semi-stmt
  -Wfloat-equal
  -Wformat
  -Winit-self
  -Wimplicit-fallthrough
  -Winconsistent-missing-destructor-override
  -Winvalid-pch
  -Wlong-long
  -Wmissing-braces
  -Wmissing-declarations
  -Wmissing-include-dirs
  -Wmissing-prototypes
  -Wnon-virtual-dtor
  -Wold-style-cast
  -Woverloaded-virtual
  -Wpacked
  #-Wpadded
  -Wpointer-arith
  -Wrange-loop-analysis
  -Wredundant-decls
  -Wredundant-move
  -Wshadow
  -Wsuggest-override
  -Wswitch-enum
  -Wthread-safety-analysis
  -Wundefined-reinterpret-cast
  -Wuninitialized
  -Wunused
  -Wzero-as-null-pointer-constant
)
# suppress warnings for external libraries built as part of include
file(GLOB_RECURSE include_files include/*)
set_source_files_properties(${include_files} PROPERTIES COMPILE_FLAGS "-w")

target_link_options(client PRIVATE
  ${opt_and_debug_linker_options}
  -lwebsocket.js
  --bind
  -sALLOW_MEMORY_GROWTH=1
  -sSTACK_SIZE=5mb
  -sWASM_BIGINT
  -sELIMINATE_DUPLICATE_FUNCTIONS=1
  -sUSE_WEBGPU
  -sUSE_GLFW=3
  -sFETCH=1
  -sUSE_FREETYPE=1
  ${exception_link_options}
  -sEXPORTED_RUNTIME_METHODS=[ccall]
  -sLLD_REPORT_UNDEFINED
  # don't emit code for node.js etc
  -sENVIRONMENT=web
  --shell-file=${CMAKE_SOURCE_DIR}/html/client.html
  # embedded files
  --preload-file ../assets@/
)

set(CMAKE_EXECUTABLE_SUFFIX ".html")
