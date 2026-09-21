#pragma once
#include <cstdint>

// Return the smallest changed interval on a row. An empty row has begin=end.
// Both buffers use the same RGB565 byte order; no color conversion is needed.
inline bool changed_span(const uint16_t *before, const uint16_t *after,
                         int width, int &begin, int &end) {
  begin = 0;
  while (begin < width && before[begin] == after[begin]) ++begin;
  end = width;
  if (begin == end) return false;
  while (end > begin && before[end - 1] == after[end - 1]) --end;
  return true;
}
