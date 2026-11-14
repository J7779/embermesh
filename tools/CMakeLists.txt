# EmberMesh host tools (native builds only; see EMBER_BUILD_TOOLS in the root CMakeLists.txt).

# --- guidepack: guide corpus validator / packer / inspector -----------------------------------
# Reuses the shared core guides code. When the core is built without the guides module
# (EMBER_MODULES subset), compile those sources straight into the tool instead.
set(_ember_guides_src ${CMAKE_SOURCE_DIR}/core/src/guides)
if(EMBER_MODULES STREQUAL "all" OR "guides" IN_LIST EMBER_MODULES)
  add_executable(guidepack guidepack/main.cpp guidepack/eval.cpp)
  target_link_libraries(guidepack PRIVATE ember_core)
else()
  file(GLOB _ember_guides_cpp CONFIGURE_DEPENDS ${_ember_guides_src}/*.cpp)
  add_executable(guidepack guidepack/main.cpp guidepack/eval.cpp ${_ember_guides_cpp})
  target_include_directories(guidepack PRIVATE ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/core/src)
endif()

# --- guides_pack: <build>/guides/guides.pack from guides/content ------------------------------
# Built as part of the normal native build. Cross builds (Android / iOS / WASM) do not build
# tools; they bundle a pack produced by a host build (pass -DEMBER_GUIDES_PACK=<path>) — see
# tools/guidepack/README.md. Succeeds for an empty or missing corpus (empty pack).
set(EMBER_GUIDES_CONTENT_DIR "${CMAKE_SOURCE_DIR}/guides/content" CACHE PATH "Guide corpus directory")
set(_ember_guides_pack "${CMAKE_BINARY_DIR}/guides/guides.pack")
file(GLOB_RECURSE _ember_guide_files CONFIGURE_DEPENDS
  "${EMBER_GUIDES_CONTENT_DIR}/*.md")
# Semantic search: the committed static embedding model (models/README.md). An empty value builds
# a keyword-only pack.
set(EMBER_GUIDES_MODEL "${CMAKE_SOURCE_DIR}/models/potion-base-4M/potion-base-4M-q4.emodel" CACHE FILEPATH
    "Static embedding model packed into guides.pack for semantic search (empty = keyword-only pack)")
set(_ember_guides_model_args "")
set(_ember_guides_model_dep "")
if(EMBER_GUIDES_MODEL AND EXISTS "${EMBER_GUIDES_MODEL}")
  set(_ember_guides_model_args --model "${EMBER_GUIDES_MODEL}")
  set(_ember_guides_model_dep "${EMBER_GUIDES_MODEL}")
endif()
add_custom_command(
  OUTPUT ${_ember_guides_pack}
  COMMAND guidepack build "${EMBER_GUIDES_CONTENT_DIR}" -o "${_ember_guides_pack}" ${_ember_guides_model_args}
  DEPENDS guidepack ${_ember_guide_files} ${_ember_guides_model_dep}
  COMMENT "Packing guides from ${EMBER_GUIDES_CONTENT_DIR}"
  VERBATIM)
# A cross build (e.g. Linux/Windows binaries built on another OS) cannot run its own guidepack
# unless CMAKE_CROSSCOMPILING_EMULATOR is set; then the pack is not part of ALL (build it natively).
if(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
  add_custom_target(guides_pack DEPENDS ${_ember_guides_pack})
else()
  add_custom_target(guides_pack ALL DEPENDS ${_ember_guides_pack})
endif()
# Consumers in the same build: get_property(p GLOBAL PROPERTY EMBER_GUIDES_PACK_FILE)
set_property(GLOBAL PROPERTY EMBER_GUIDES_PACK_FILE ${_ember_guides_pack})

# Search-quality gate on the real corpus (tools/guidepack/README.md, "Search quality"): fails if
# hybrid ranking falls below the thresholds measured for the committed model. Not part of the
# fast unit-test suite; run with `ctest -R guides_search_quality` after building guides_pack.
add_test(NAME guides_search_quality
  COMMAND guidepack eval "${_ember_guides_pack}"
          --queries "${CMAKE_SOURCE_DIR}/tools/guidepack/eval/queries.tsv"
          --thresholds "${CMAKE_SOURCE_DIR}/tools/guidepack/eval/thresholds.txt")
set_tests_properties(guides_search_quality PROPERTIES LABELS "guides;quality")

# --- guides_bench: search latency / memory on a real pack (tools/guidepack/README.md) ---------
if(EMBER_MODULES STREQUAL "all" OR "guides" IN_LIST EMBER_MODULES)
  add_executable(guides_bench guidepack/bench.cpp)
  target_link_libraries(guides_bench PRIVATE ember_core)
else()
  add_executable(guides_bench guidepack/bench.cpp ${_ember_guides_cpp})
  target_include_directories(guides_bench PRIVATE ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/core/src)
endif()
