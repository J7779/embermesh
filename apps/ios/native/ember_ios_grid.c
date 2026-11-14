/* See ember_ios_grid.h. */
#include "ember_ios_grid.h"

#include <string.h>

int ember_ios_luma_grid(const uint8_t* plane, size_t width, size_t height, size_t bpr,
                        unsigned gw, unsigned gh, uint8_t* max_out, uint8_t* mean_out) {
  if (!plane || !max_out || gw == 0 || gh == 0 || gw > width || gh > height || (size_t)gw * gh > 4096 || bpr < width)
    return -1;
  size_t x0s[4097];
  for (unsigned c = 0; c <= gw; ++c) x0s[c] = (size_t)c * width / gw;
  uint32_t sums[4096];
  for (unsigned r = 0; r < gh; ++r) {
    size_t y0 = (size_t)r * height / gh, y1 = (size_t)(r + 1) * height / gh;
    uint8_t* mx = max_out + (size_t)r * gw;
    memset(mx, 0, gw);
    memset(sums, 0, sizeof(uint32_t) * gw);
    for (size_t y = y0; y < y1; ++y) {
      const uint8_t* row = plane + y * bpr;
      for (unsigned c = 0; c < gw; ++c) {
        const uint8_t* p = row + x0s[c];
        size_t n = x0s[c + 1] - x0s[c];
        uint8_t m = mx[c];
        uint32_t s = 0;
        for (size_t i = 0; i < n; ++i) {   /* vectorised: max + widening sum */
          uint8_t v = p[i];
          m = v > m ? v : m;
          s += v;
        }
        mx[c] = m;
        sums[c] += s;
      }
    }
    if (mean_out) {
      for (unsigned c = 0; c < gw; ++c) {
        size_t cnt = (x0s[c + 1] - x0s[c]) * (y1 - y0);
        mean_out[(size_t)r * gw + c] = (uint8_t)(cnt ? (sums[c] + cnt / 2) / cnt : 0);
      }
    }
  }
  return 0;
}
