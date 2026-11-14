# EmberMesh native host layer (ember_host).
#
# Sockets, event loop, HTTP/1.1 + WebSocket server/client, the hub (port 8437), endpoints
# (tcp, tcp_server, udp multicast, websocket, ws_server, serial, audio, rtl_tcp, loopback) and the
# desktop host router for host.* RPC methods. Uses the core ONLY through <ember/ember.h>.
#
# No dependencies beyond the OS; vendored header-only code lives in host/third_party/.
#
# Options (all platforms build sockets + hub; turn hardware pieces off for mobile):
#   EMBER_HOST_AUDIO   miniaudio sound-card endpoint     (default ON on desktop, OFF on Android/iOS)
#   EMBER_HOST_SERIAL  serial-port endpoint + PTT lines  (default ON on desktop, OFF on Android/iOS)
#   EMBER_HOST_TESTS   host unit/integration tests       (default = EMBER_BUILD_TESTS or ON)

set(_host_hw ON)
if(ANDROID OR IOS OR EMSCRIPTEN)
  set(_host_hw OFF)
endif()
option(EMBER_HOST_AUDIO  "Build the miniaudio sound-card endpoint" ${_host_hw})
option(EMBER_HOST_SERIAL "Build the serial-port endpoint"          ${_host_hw})
if(DEFINED EMBER_BUILD_TESTS)
  set(_host_tests_default ${EMBER_BUILD_TESTS})
else()
  set(_host_tests_default ON)
endif()
option(EMBER_HOST_TESTS "Build host tests (ember_host_tests)" ${_host_tests_default})

find_package(Threads REQUIRED)

file(GLOB_RECURSE _host_sources CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
# Sources live in src/host/endpoints/. (Until this was fixed the patterns did not match, so OFF still
# compiled miniaudio and the IOKit/termios serial code — which breaks iOS builds.)
if(NOT EMBER_HOST_AUDIO)
  list(FILTER _host_sources EXCLUDE REGEX ".*/endpoints/audio[^/]*\\.(c|cpp)$")
endif()
if(NOT EMBER_HOST_SERIAL)
  list(FILTER _host_sources EXCLUDE REGEX ".*/endpoints/serial[^/]*\\.(c|cpp)$")
endif()
message(STATUS "ember_host: audio=${EMBER_HOST_AUDIO} serial=${EMBER_HOST_SERIAL}")

add_library(ember_host STATIC ${_host_sources})
target_include_directories(ember_host
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include     # <ember_host.h>
          ${CMAKE_CURRENT_SOURCE_DIR}/src         # "host/..." C++ API used by emberd + tests
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(ember_host PUBLIC ember_core Threads::Threads)
target_compile_definitions(ember_host PUBLIC
  EMBER_HOST_VERSION="${PROJECT_VERSION}"
  $<$<BOOL:${EMBER_HOST_AUDIO}>:EMBER_HOST_HAVE_AUDIO=1>
  $<$<BOOL:${EMBER_HOST_SERIAL}>:EMBER_HOST_HAVE_SERIAL=1>)

# Our own sources get warnings; the vendored miniaudio translation unit does not.
set(_host_own ${_host_sources})
list(FILTER _host_own EXCLUDE REGEX ".*miniaudio[^/]*\\.c$")
if(MSVC)
  set_source_files_properties(${_host_own} PROPERTIES COMPILE_OPTIONS "/W3")
  target_compile_definitions(ember_host PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  set_source_files_properties(${_host_own} PROPERTIES COMPILE_OPTIONS
    "-Wall;-Wextra;-Wshadow;-Wno-unused-parameter")
  # Vendored miniaudio (kept unmodified) ignores read()'s result in its ALSA/PulseAudio wakeup
  # paths; glibc marks read() warn_unused_result, so GCC warns by default.
  set(_host_vendored ${_host_sources})
  list(FILTER _host_vendored INCLUDE REGEX ".*miniaudio[^/]*\\.c$")
  if(_host_vendored)
    set_source_files_properties(${_host_vendored} PROPERTIES COMPILE_OPTIONS "-Wno-unused-result")
  endif()
endif()

if(WIN32)
  target_link_libraries(ember_host PUBLIC ws2_32 iphlpapi bcrypt advapi32)
elseif(APPLE)
  if(EMBER_HOST_AUDIO)
    target_link_libraries(ember_host PUBLIC "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
    if(NOT IOS)
      target_link_libraries(ember_host PUBLIC "-framework IOKit")  # CM108 HID PTT
    endif()
  endif()
elseif(UNIX)
  target_link_libraries(ember_host PUBLIC ${CMAKE_DL_LIBS})
  find_library(EMBER_HOST_LIBM m)
  if(EMBER_HOST_LIBM)
    target_link_libraries(ember_host PUBLIC ${EMBER_HOST_LIBM})
  endif()
endif()

if(EMBER_HOST_TESTS)
  file(GLOB _host_tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  if(NOT EMBER_HOST_SERIAL)
    list(FILTER _host_tests EXCLUDE REGEX ".*/serial_test\\.cpp$")
  endif()
  add_executable(ember_host_tests ${_host_tests})
  # doctest prints values through std::ostream: let it include the real standard headers instead of
  # forward-declaring them (MSVC's <string_view> needs the complete type).
  target_compile_definitions(ember_host_tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
  target_link_libraries(ember_host_tests PRIVATE ember_host)
  target_include_directories(ember_host_tests PRIVATE ${CMAKE_SOURCE_DIR}/core/third_party/include)
  target_compile_definitions(ember_host_tests PRIVATE EMBER_HOST_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_test(NAME ember_host_tests COMMAND ember_host_tests)
endif()
