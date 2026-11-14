# EmberMesh native host layer (ember_host).
#
# Sockets, event loop, HTTP/1.1 + WebSocket server/client, the hub (port 8437), endpoints
# (tcp, tcp_server, udp multicast, websocket, ws_server, serial, audio, rtl_tcp, loopback) and the
# desktop host router for host.* RPC methods. Uses the core ONLY through <ember/ember.h>.
#
# No dependencies beyond the OS; vendored code lives in host/third_party/ (miniaudio, mbedTLS).
#
# Options (all platforms build sockets + hub; turn hardware pieces off for mobile):
#   EMBER_HOST_AUDIO   miniaudio sound-card endpoint     (default ON on desktop, OFF on Android/iOS)
#   EMBER_HOST_SERIAL  serial-port endpoint + PTT lines  (default ON on desktop, OFF on Android/iOS)
#   EMBER_HOST_MDNS    mDNS Nearby discovery              (default ON on desktop, OFF on Android/iOS)
#   EMBER_HOST_TLS     wss:// client on vendored mbedTLS  (default ON on desktop, OFF on Android/iOS)
#   EMBER_HOST_TESTS   host unit/integration tests       (default = EMBER_BUILD_TESTS or ON)

set(_host_hw ON)
if(ANDROID OR IOS OR EMSCRIPTEN)
  set(_host_hw OFF)
endif()
option(EMBER_HOST_AUDIO  "Build the miniaudio sound-card endpoint" ${_host_hw})
option(EMBER_HOST_SERIAL "Build the serial-port endpoint"          ${_host_hw})
option(EMBER_HOST_MDNS   "Build mDNS Nearby discovery (desktop)"   ${_host_hw})
# TLS for wss:// relay links (desktop): vendored mbedTLS 3.6 LTS, client-only TLS 1.2 build
# (host/third_party/mbedtls; configuration in src/host/tls_mbedtls_config.h). The phones use their
# own platform TLS, so it is off there.
# A build directory configured while this option still defaulted to OFF adopts the new default once.
if(DEFINED CACHE{EMBER_HOST_TLS} AND NOT DEFINED CACHE{_EMBER_HOST_TLS_DEFAULT_ON})
  set_property(CACHE EMBER_HOST_TLS PROPERTY VALUE ${_host_hw})
endif()
set(_EMBER_HOST_TLS_DEFAULT_ON ON CACHE INTERNAL "EMBER_HOST_TLS default migrated")
option(EMBER_HOST_TLS    "Build the TLS WebSocket client (vendored mbedTLS)" ${_host_hw})
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
if(NOT EMBER_HOST_TLS)
  list(FILTER _host_sources EXCLUDE REGEX ".*/host/tls[^/]*\\.cpp$")
endif()
if(NOT EMBER_HOST_MDNS)
  list(FILTER _host_sources EXCLUDE REGEX ".*/host/(mdns|nearby)\\.cpp$")
endif()
message(STATUS "ember_host: audio=${EMBER_HOST_AUDIO} serial=${EMBER_HOST_SERIAL} mdns=${EMBER_HOST_MDNS} tls=${EMBER_HOST_TLS}")

add_library(ember_host STATIC ${_host_sources})

if(EMBER_HOST_TLS)
  # mbedTLS 3.6.2 (upstream tarball SHA-256 8b54fb9bcf4d5a7078028e0520acddefb7900b3e66fec7f7175bb5b7d85ccdca):
  # only the library sources this client configuration compiles, and the headers they include. Not
  # vendored: tests, programs, scripts, the default config and every other library source. One local
  # change: rsa.c without its MBEDTLS_SELF_TEST block (embedded test key pair). Our configuration is
  # selected with MBEDTLS_CONFIG_FILE.
  file(GLOB _mbedtls_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mbedtls/library/*.c)
  add_library(ember_mbedtls STATIC ${_mbedtls_sources})
  target_include_directories(ember_mbedtls
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mbedtls/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mbedtls/library ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(ember_mbedtls PUBLIC "MBEDTLS_CONFIG_FILE=\"host/tls_mbedtls_config.h\"")
  target_include_directories(ember_mbedtls PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  if(MSVC)
    target_compile_options(ember_mbedtls PRIVATE /W0)
  else()
    target_compile_options(ember_mbedtls PRIVATE -w)
  endif()
  target_link_libraries(ember_host PUBLIC ember_mbedtls)
  if(WIN32)
    target_link_libraries(ember_host PUBLIC crypt32)
  elseif(APPLE)
    target_link_libraries(ember_host PUBLIC "-framework Security" "-framework CoreFoundation")
  endif()
endif()
target_include_directories(ember_host
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include     # <ember_host.h>
          ${CMAKE_CURRENT_SOURCE_DIR}/src         # "host/..." C++ API used by emberd + tests
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(ember_host PUBLIC ember_core Threads::Threads)
target_compile_definitions(ember_host PUBLIC
  EMBER_HOST_VERSION="${PROJECT_VERSION}"
  $<$<BOOL:${EMBER_HOST_AUDIO}>:EMBER_HOST_HAVE_AUDIO=1>
  $<$<BOOL:${EMBER_HOST_SERIAL}>:EMBER_HOST_HAVE_SERIAL=1>
  $<$<BOOL:${EMBER_HOST_MDNS}>:EMBER_HOST_HAVE_MDNS=1>
  $<$<BOOL:${EMBER_HOST_TLS}>:EMBER_HOST_HAVE_TLS=1>)

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
