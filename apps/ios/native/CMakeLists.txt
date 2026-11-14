# Native libraries linked into the iOS app (built by apps/ios/build-core.sh, never by hand-listed
# sources in the Xcode project):
#   ember_core      core/ (all modules by default; sources globbed by core/CMakeLists.txt)
#   ember_host      host/ (sockets + HTTP/WebSocket hub), audio and serial endpoints OFF
#   ember_ios_hub   ember_ios_hub.cpp: C wrapper that embeds the host library's Hub as the
#                   `ws_server` endpoint (see ember_ios_hub.h)
# build-core.sh merges the three into one libember_ios.a per SDK.
cmake_minimum_required(VERSION 3.20)

get_filename_component(EMBER_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
# Same version as the top-level project (core and host bake PROJECT_VERSION into their libraries).
file(STRINGS "${EMBER_REPO}/CMakeLists.txt" _ember_project REGEX "^project\\(EmberMesh VERSION")
string(REGEX MATCH "VERSION ([0-9.]+)" _m "${_ember_project}")
set(_ember_version "${CMAKE_MATCH_1}")
if(NOT _ember_version)
  set(_ember_version 0.1.0)
endif()
project(EmberMeshIOS VERSION ${_ember_version} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Path-free outputs (same rule as the top-level CMakeLists.txt, which this project does not include):
# never embed the builder's checkout path in __FILE__ strings or debug info.
add_compile_options("-ffile-prefix-map=${EMBER_REPO}=." "-fdebug-prefix-map=${EMBER_REPO}=.")

set(EMBER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(EMBER_HOST_TESTS OFF CACHE BOOL "" FORCE)
set(EMBER_HOST_AUDIO OFF CACHE BOOL "" FORCE)
set(EMBER_HOST_SERIAL OFF CACHE BOOL "" FORCE)
option(EMBER_IOS_HUB "Build the host library + hub wrapper" ON)

add_subdirectory(${EMBER_REPO}/core ${CMAKE_BINARY_DIR}/core)

if(EMBER_IOS_HUB)
  add_subdirectory(${EMBER_REPO}/host ${CMAKE_BINARY_DIR}/host)
  # host/CMakeLists.txt drops audio/serial sources with the regex ".*/src/endpoints/audio…", but the
  # files live in src/host/endpoints/, so with EMBER_HOST_AUDIO/SERIAL=OFF they are still compiled
  # (miniaudio pulls AVFoundation into a C file; serial needs IOKit/serial, absent on iOS). Remove
  # them here until the host library's filter matches.
  get_target_property(_host_srcs ember_host SOURCES)
  list(FILTER _host_srcs EXCLUDE REGEX ".*/endpoints/(audio|serial)[^/]*\\.(c|cpp)$")
  set_property(TARGET ember_host PROPERTY SOURCES ${_host_srcs})
  add_library(ember_ios_hub STATIC ${CMAKE_CURRENT_SOURCE_DIR}/ember_ios_hub.cpp)
  target_link_libraries(ember_ios_hub PUBLIC ember_host)
  target_include_directories(ember_ios_hub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
