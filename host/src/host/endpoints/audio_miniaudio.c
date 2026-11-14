/* The single miniaudio implementation translation unit (vendored header kept unmodified in
 * host/third_party/miniaudio/, public domain / MIT-0). Only the low-level device API is used. */
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"
