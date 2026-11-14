# EmberMesh — WebAssembly build of the shared core (Emscripten only).
#
# Included by the root CMakeLists.txt when configured with emcmake. Produces, in
# ${CMAKE_BINARY_DIR}/wasm/:
#   ember.mjs + ember.wasm   ES-module loader + separate wasm (static site / PWA)
#   ember.single.mjs          same module with the wasm embedded (single-file embermesh.html)
#
# Both export the complete C API of core/include/ember/ember.h plus malloc/free, the heap views and
# the FS / IDBFS objects the JS binding (ui/js/backends/wasm.js) needs. See apps/web/README.md.

if(NOT EMSCRIPTEN)
  return()
endif()

set(EMBER_WEB_OUT_DIR ${CMAKE_BINARY_DIR}/wasm CACHE PATH "Output directory of the WASM modules")
set(EMBER_WEB_STACK_SIZE 524288 CACHE STRING "WASM stack size in bytes")
# "simd": the build dir is configured with -msimd128 (apps/web/build.sh does this in build/web-simd)
# and produces ember-simd.mjs/.wasm only; the page picks it after a feature test (wasm.js).
set(EMBER_WEB_VARIANT "" CACHE STRING "WASM module variant: empty (baseline) or simd")

set(_ember_exports
  _ember_version _ember_create _ember_destroy _ember_add_entropy
  _ember_call _ember_free _ember_tick _ember_next_wakeup_ms
  _ember_link_rx _ember_audio_rx _ember_audio_tx _ember_poll _ember_guides_load
  _ember_web_out_size _ember_web_out_data_offset _ember_web_out_len_offset
  _malloc _free)
list(JOIN _ember_exports "," _ember_exports_csv)

set(_ember_runtime
  HEAPU8 HEAP16 HEAP32 HEAPU32 wasmMemory
  UTF8ToString stringToUTF8 lengthBytesUTF8
  FS IDBFS)
list(JOIN _ember_runtime "," _ember_runtime_csv)

# Common link flags. No pthreads (works without COOP/COEP headers and from file://), memory growth for
# large guide packs, no C++ exceptions, emmalloc for size (uint64_t now_ms is passed as BigInt).
set(_ember_web_link_flags
  -sMODULARIZE=1
  -sEXPORT_ES6=1
  -sEXPORT_NAME=createEmberModule
  -sENVIRONMENT=web,worker,node
  -sALLOW_MEMORY_GROWTH=1
  -sINITIAL_MEMORY=16777216
  -sSTACK_SIZE=${EMBER_WEB_STACK_SIZE}
  -sMALLOC=emmalloc
  -sDISABLE_EXCEPTION_CATCHING=1
  -sFILESYSTEM=1
  -sFORCE_FILESYSTEM=1
  -lidbfs.js
  -sINCOMING_MODULE_JS_API=print,printErr,locateFile,wasmBinary,instantiateWasm,noInitialRun
  -sEXPORTED_FUNCTIONS=${_ember_exports_csv}
  -sEXPORTED_RUNTIME_METHODS=${_ember_runtime_csv}
  -sTEXTDECODER=2
  -fno-exceptions)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND _ember_web_link_flags -sASSERTIONS=1 -g)
elseif(EMBER_WEB_VARIANT STREQUAL "simd")
  # the SIMD module is for speed (loop vectorisation needs -O2/-O3; at -Os it gains nothing)
  list(APPEND _ember_web_link_flags -sASSERTIONS=0 -O3)
else()
  list(APPEND _ember_web_link_flags -sASSERTIONS=0 -Os)
endif()

function(ember_web_module target output_name)
  add_executable(${target} ${CMAKE_CURRENT_SOURCE_DIR}/src/ember_web.c)
  target_link_libraries(${target} PRIVATE ember_core)
  target_link_options(${target} PRIVATE ${_ember_web_link_flags} ${ARGN})
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME ${output_name}
    SUFFIX ".mjs"
    RUNTIME_OUTPUT_DIRECTORY ${EMBER_WEB_OUT_DIR})
endfunction()

if(EMBER_WEB_VARIANT STREQUAL "simd")
  ember_web_module(ember_wasm ember-simd)
  add_custom_target(ember_web ALL DEPENDS ember_wasm)
else()
  ember_web_module(ember_wasm ember)
  # One-file variant: wasm embedded as base64 (the binary text encoding is not safe to embed inside an
  # HTML <script> element, whose parser rewrites NUL and CR characters).
  ember_web_module(ember_wasm_single ember.single -sSINGLE_FILE=1 -sSINGLE_FILE_BINARY_ENCODE=0)
  add_custom_target(ember_web ALL DEPENDS ember_wasm ember_wasm_single)
endif()
