# Android JNI library: links the shared EmberMesh core (built from the repository root CMake) and the
# host library's hub (host/, sockets + HTTP + WebSocket; audio and serial off) into libember_jni.so.
# The repo root is passed by Gradle as EMBER_ROOT.
cmake_minimum_required(VERSION 3.20)
if(NOT EMBER_VERSION)
  set(EMBER_VERSION 0.1.0)
endif()
project(EmberAndroid VERSION ${EMBER_VERSION} LANGUAGES C CXX)

if(NOT EMBER_ROOT)
  get_filename_component(EMBER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)
endif()
# No builder-specific absolute paths in the binaries (__FILE__, debug info): repository root -> ".".
add_compile_options("-ffile-prefix-map=${EMBER_ROOT}=." "-fdebug-prefix-map=${EMBER_ROOT}=.")

option(EMBER_ANDROID_HUB "Embed the host library's hub (host.hub.*, ws_server endpoint)" ON)

# The root CMakeLists builds only ember_core when ANDROID is set (no tests/tools/desktop).
add_subdirectory(${EMBER_ROOT} ember)

set(_jni_sources ember_jni.cpp)
if(EMBER_ANDROID_HUB AND EXISTS ${EMBER_ROOT}/host/CMakeLists.txt)
  # Mobile build of the host library: no miniaudio, no serial ports, no tests.
  set(EMBER_HOST_AUDIO OFF CACHE BOOL "" FORCE)
  set(EMBER_HOST_SERIAL OFF CACHE BOOL "" FORCE)
  set(EMBER_HOST_TESTS OFF CACHE BOOL "" FORCE)
  add_subdirectory(${EMBER_ROOT}/host ember_host)
  # Hub-only C API (host/include/ember_hub.h): the Kotlin node runner keeps driving the node.
  list(APPEND _jni_sources hub_jni.cpp)
  set(_have_hub ON)
endif()

add_library(ember_jni SHARED ${_jni_sources})
target_link_libraries(ember_jni PRIVATE ember_core android log)
if(_have_hub)
  target_link_libraries(ember_jni PRIVATE ember_host)
  target_compile_definitions(ember_jni PRIVATE EMBER_ANDROID_HAVE_HUB=1)
endif()
target_compile_options(ember_jni PRIVATE -Wall -Wextra -Wshadow -Werror -fno-exceptions -fno-rtti)
# 16 KB page alignment (required for Android 15+ devices with 16 KB pages); drop unused code.
target_link_options(ember_jni PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,--gc-sections" "-Wl,--exclude-libs,ALL")
target_compile_options(ember_jni PRIVATE -ffunction-sections -fdata-sections)
