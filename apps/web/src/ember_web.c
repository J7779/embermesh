/* EmberMesh WebAssembly entry translation unit.
 *
 * The module has no main(): JS drives the node through the C API in ember/ember.h, which is
 * linked in from ember_core and exported by apps/web/CMakeLists.txt. The two helpers below let the
 * JS binding verify the ember_out struct layout at runtime instead of hard-coding assumptions. */
#include <stddef.h>
#include "ember/ember.h"

EMBER_API size_t ember_web_out_size(void) { return sizeof(ember_out); }
EMBER_API size_t ember_web_out_data_offset(void) { return offsetof(ember_out, data); }
EMBER_API size_t ember_web_out_len_offset(void) { return offsetof(ember_out, len); }
