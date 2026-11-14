# emberd: node + hub + radio gateway + terminal chat client in one executable.
add_library(emberd_console STATIC console.cpp remote_rpc.cpp)
target_link_libraries(emberd_console PUBLIC ember_host)
target_include_directories(emberd_console PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(emberd main.cpp)
target_link_libraries(emberd PRIVATE emberd_console ember_host)
if(NOT MSVC)
  target_compile_options(emberd PRIVATE -Wall -Wextra -Wshadow -Wno-unused-parameter)
  target_compile_options(emberd_console PRIVATE -Wall -Wextra -Wshadow -Wno-unused-parameter)
endif()
if(WIN32)
  target_link_libraries(emberd PRIVATE shell32)
endif()

install(TARGETS emberd RUNTIME DESTINATION bin)

if(EMBER_HOST_TESTS)
  add_executable(emberd_tests tests/console_test.cpp ${CMAKE_SOURCE_DIR}/host/tests/main.cpp)
  # doctest prints values through std::ostream: let it include the real standard headers instead of
  # forward-declaring them (MSVC's <string_view> needs the complete type).
  target_compile_definitions(emberd_tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
  target_link_libraries(emberd_tests PRIVATE emberd_console)
  target_include_directories(emberd_tests PRIVATE ${CMAKE_SOURCE_DIR}/core/third_party/include)
  add_test(NAME emberd_tests COMMAND emberd_tests)
endif()

# End-to-end mesh tests: real emberd processes over real sockets, ptys, FIFOs (POSIX only).
if(EMBER_HOST_TESTS AND NOT WIN32)
  add_executable(emberd_mesh_tests tests/mesh_test.cpp ${CMAKE_SOURCE_DIR}/host/tests/main.cpp)
  # doctest prints values through std::ostream: let it include the real standard headers instead of
  # forward-declaring them (MSVC's <string_view> needs the complete type).
  target_compile_definitions(emberd_mesh_tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
  target_link_libraries(emberd_mesh_tests PRIVATE emberd_console ember_host)
  if(NOT APPLE)
    target_link_libraries(emberd_mesh_tests PRIVATE util)
  endif()
  target_include_directories(emberd_mesh_tests PRIVATE ${CMAKE_SOURCE_DIR}/core/third_party/include)
  target_compile_definitions(emberd_mesh_tests PRIVATE EMBERD_PATH="$<TARGET_FILE:emberd>"
                                                       EMBER_REPO_DIR="${CMAKE_SOURCE_DIR}")
  add_dependencies(emberd_mesh_tests emberd)
  # One CTest entry per scenario so `ctest -j` runs them in parallel.
  foreach(_case
      "udp multicast pair"
      "tcp_server and tcp pair"
      "websocket client to hub"
      "chain relay and store-carry-forward"
      "gateway hub to kiss radio link"
      "gateway hub to audio modem link over a virtual audio pipe"
      "serial over a pty cable with stream and kiss drivers"
      "rtl_tcp receive of real modulated IQ"
      "daemon survives garbage on every endpoint"
      "ws flood and slow guest eviction"
      "100 simultaneous guests"
      "clean shutdown and restart keep identity links and messages"
      "locked store at start"
      "console attach and in-process console against the real core"
      "handheld_vox preset through a simulated VOX radio"
      "acoustic preset through a degraded audio path"
      "native morse capture from a PCM pipe and the console"
      "rpc carries 30 feeds per second without blocking other calls"
      "big guide pack mmap with ranges ETag and many downloads"
      "a fresh identity is anonymous by default"
      "nearby devices find each other by mDNS with no links configured"
      "internet relay over wss through a TLS proxy to the built-in broker")
    string(REGEX REPLACE "[^A-Za-z0-9]+" "_" _id "${_case}")
    add_test(NAME emberd_mesh.${_id} COMMAND emberd_mesh_tests "--test-case=${_case}")
    set_tests_properties(emberd_mesh.${_id} PROPERTIES TIMEOUT 180 LABELS mesh)
    if(_case MATCHES "acoustic|handheld_vox")
      set_tests_properties(emberd_mesh.${_id} PROPERTIES TIMEOUT 600)  # robust modem: ~15 s per frame
    endif()
  endforeach()
endif()
