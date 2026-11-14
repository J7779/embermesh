/* Whole-frame brightness grid for the Morse camera reader (docs/RPC_API.md section 12).
 *
 * Reduces an 8-bit luma plane (the Y plane of a 420 pixel buffer, full resolution, sensor
 * orientation) to gw x gh cells: max_out[cell] = the LARGEST luma of the pixels the cell covers
 * (max-pooling, so a light covering a single pixel decides its cell), mean_out[cell] = the average
 * (optional, NULL to skip). Cells are row-major from the top-left of the buffer; cell boundaries are
 * floor(i * size / cells). Compiled with -O3 (auto-vectorised) even in Debug builds. */
#ifndef EMBER_IOS_GRID_H
#define EMBER_IOS_GRID_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 0, or -1 for bad arguments (gw/gh 0 or larger than the plane, gw*gh > 4096). */
int ember_ios_luma_grid(const uint8_t* plane, size_t width, size_t height, size_t bytes_per_row,
                        unsigned gw, unsigned gh, uint8_t* max_out, uint8_t* mean_out);

#ifdef __cplusplus
}
#endif

#endif
