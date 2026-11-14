cmake_minimum_required(VERSION 3.20)
project(EmberMesh VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Cross builds (Android NDK, iOS, Emscripten) only build the core library by default.
set(_ember_native ON)
if(CMAKE_CROSSCOMPILING OR EMSCRIPTEN OR ANDROID OR IOS)
  set(_ember_native OFF)
endif()

option(EMBER_BUILD_TESTS   "Build core unit tests (ember_tests)"            ${_ember_native})
option(EMBER_BUILD_TOOLS   "Build host tools (guide packer, ...)"           ${_ember_native})
option(EMBER_BUILD_DESKTOP "Build host layer + desktop daemon/CLI"          ${_ember_native})
option(EMBER_BUILD_WEB     "Build the WebAssembly module (Emscripten only)" ON)
option(EMBER_SANITIZE      "Enable ASan + UBSan (native debug builds)"      OFF)

if(EMBER_SANITIZE)
  # -fno-sanitize=vptr: the core is built with -fno-rtti, so UBSan's vptr check (on by default with
  # GCC) would make RTTI-enabled test code reference typeinfo that the core never emits (link error).
  add_compile_options(-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined -fno-sanitize=vptr)
endif()

# --- Portability ---------------------------------------------------------------------------------
# Sources are UTF-8 without BOM (string literals contain UTF-8); MSVC would otherwise read them in
# the system code page.
# /bigobj: the larger doctest translation units can exceed the default COFF section limit (C1128).
if(MSVC)
  add_compile_options(/utf-8 /bigobj)
elseif(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")  # GNU as; clang switches to big-obj itself
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wa,-mbig-obj>)
endif()
# 64-bit off_t / fseeko / pread on 32-bit Linux (Raspberry Pi OS armhf). Not for Android, where
# _FILE_OFFSET_BITS=64 hides functions below API 24.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
  add_compile_definitions(_FILE_OFFSET_BITS=64)
endif()

# Self-contained desktop executables: link the C++ runtime statically where that is safe.
#   Windows/MSVC: static CRT (/MT, /MTd) -> no VC++ redistributable needed.
#   Windows/MinGW: -static -> no libstdc++/libgcc/libwinpthread DLLs next to the .exe.
#   Linux: -static-libstdc++ -static-libgcc (glibc itself stays dynamic: NSS, getaddrinfo).
# macOS/iOS/Android/WASM use the platform's C++ runtime and are unaffected.
set(_ember_static_default OFF)
if(WIN32 OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID))
  set(_ember_static_default ON)
endif()
option(EMBER_STATIC_RUNTIME "Link the C/C++ runtime statically into executables (Windows, Linux)" ${_ember_static_default})
if(EMBER_STATIC_RUNTIME AND NOT EMBER_SANITIZE)
  if(MSVC)
    if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
  elseif(MINGW)
    add_link_options(-static)
  elseif(NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_link_options(-static-libstdc++ -static-libgcc)
  endif()
endif()

# Reproducible, path-free outputs: never embed the builder's checkout path (and with it the home
# directory) in __FILE__ strings, debug info or the macOS linker's object-file debug map. Only the
# source/build prefixes are rewritten; test-only compile definitions keep their absolute values.
# CMAKE_CURRENT_SOURCE_DIR is the repository root even when an app project adds this directory.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=." EMBER_HAS_FILE_PREFIX_MAP)
if(EMBER_HAS_FILE_PREFIX_MAP)
  add_compile_options("-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=." "-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
  if(NOT CMAKE_BINARY_DIR MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}/")
    add_compile_options("-ffile-prefix-map=${CMAKE_BINARY_DIR}=build" "-fdebug-prefix-map=${CMAKE_BINARY_DIR}=build")
  endif()
endif()
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  include(CheckLinkerFlag OPTIONAL RESULT_VARIABLE _ember_has_check_linker)
  if(_ember_has_check_linker)
    check_linker_flag(CXX "LINKER:-oso_prefix,${CMAKE_CURRENT_SOURCE_DIR}/" EMBER_HAS_OSO_PREFIX)
    if(EMBER_HAS_OSO_PREFIX)
      add_link_options("LINKER:-oso_prefix,${CMAKE_CURRENT_SOURCE_DIR}/")
    endif()
  endif()
endif()

enable_testing()

add_subdirectory(core)

# Each of these directories is optional so partial checkouts / in-progress trees still configure.
if(EMBER_BUILD_TOOLS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt)
  add_subdirectory(tools)
endif()
if(EMBER_BUILD_DESKTOP AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/host/CMakeLists.txt)
  add_subdirectory(host)
endif()
if(EMBER_BUILD_DESKTOP AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps/desktop/CMakeLists.txt)
  add_subdirectory(apps/desktop)
endif()
if(EMSCRIPTEN AND EMBER_BUILD_WEB AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps/web/CMakeLists.txt)
  add_subdirectory(apps/web)
endif()
