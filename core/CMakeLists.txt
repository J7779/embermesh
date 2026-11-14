# EmberMesh shared core.
#
# Sources are discovered by glob so that modules can be added without editing this file:
#   core/src/<module>/**/*.{cpp,c}      -> compiled into ember_core
#   core/tests/<module>/**/*.cpp        -> compiled into ember_tests (doctest)
#   core/third_party/<lib>/*.{c,cpp}    -> vendored compiled libraries (always built)
#   core/third_party/include/           -> vendored header-only libraries
#
# EMBER_MODULES selects which modules are built ("all" or a ;-list such as "base;fec;framing").
# A subset builds faster when you are working on a few modules (list their dependencies too).

set(EMBER_MODULES "all" CACHE STRING "Core modules to build: 'all' or a ;-separated list of dirs under core/src")

set(_src_root ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(_test_root ${CMAKE_CURRENT_SOURCE_DIR}/tests)

if(EMBER_MODULES STREQUAL "all")
  file(GLOB _children RELATIVE ${_src_root} ${_src_root}/*)
  set(_modules "")
  foreach(_c ${_children})
    if(IS_DIRECTORY ${_src_root}/${_c})
      list(APPEND _modules ${_c})
    endif()
  endforeach()
else()
  set(_modules ${EMBER_MODULES})
endif()
message(STATUS "EmberMesh core modules: ${_modules}")

set(_sources "")
set(_test_sources "")
foreach(_m ${_modules})
  file(GLOB_RECURSE _s CONFIGURE_DEPENDS ${_src_root}/${_m}/*.cpp ${_src_root}/${_m}/*.c)
  list(APPEND _sources ${_s})
  file(GLOB_RECURSE _t CONFIGURE_DEPENDS ${_test_root}/${_m}/*.cpp)
  list(APPEND _test_sources ${_t})
endforeach()

file(GLOB _third_party CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/*/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/*/*.cpp)

add_library(ember_core STATIC ${_sources} ${_third_party})
target_include_directories(ember_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include      # <ember/ember.h>, <ember/link.h>, <ember/bytes.h>, <ember/crc.h>
  ${CMAKE_CURRENT_SOURCE_DIR}/src          # "<module>/<header>.h"
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party) # "<lib>/<header>.h"
target_compile_definitions(ember_core PUBLIC EMBER_VERSION="${PROJECT_VERSION}")

# The core never throws and never uses RTTI: it must stay small and predictable on WASM / NDK / iOS
# and must survive arbitrary garbage from the radio. Enforced on our own sources (not vendored code).
if(NOT MSVC)
  set_source_files_properties(${_sources} PROPERTIES COMPILE_OPTIONS
    "-Wall;-Wextra;-Wshadow;-Wno-unused-parameter;$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>;$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>")
  if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT WIN32)
    find_library(EMBER_LIBM m)
    if(EMBER_LIBM)
      target_link_libraries(ember_core PUBLIC ${EMBER_LIBM})
    endif()
  endif()
else()
  # _HAS_EXCEPTIONS=0 only on the core's own sources: it is not PUBLIC, because the doctest test
  # executables, the host library and the tools are built with exceptions (/EHsc) and doctest
  # needs the full <exception> (uncaught_exceptions, set_terminate) that it would remove.
  set_source_files_properties(${_sources} PROPERTIES COMPILE_OPTIONS "/W3;/GR-"
                                                     COMPILE_DEFINITIONS "_HAS_EXCEPTIONS=0")
  target_compile_definitions(ember_core PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

if(EMBER_BUILD_TESTS)
  add_executable(ember_tests ${_test_root}/main.cpp ${_test_sources})
  target_link_libraries(ember_tests PRIVATE ember_core)
  target_include_directories(ember_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include ${_test_root})
  target_compile_definitions(ember_tests PRIVATE
    EMBER_TEST_DATA_DIR="${_test_root}/data"
    EMBER_REPO_ROOT="${CMAKE_SOURCE_DIR}")
  add_test(NAME ember_core_tests COMMAND ember_tests)
endif()
