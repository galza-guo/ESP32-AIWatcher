#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>

// Accumulate damage across tiles until a frame is presented. Synchronize the
// retired buffer only after scanout has released it.
template<int Width, int Height> class FrameDamage {
  uint16_t begin_[Height];
  uint16_t end_[Height];
 public:
  FrameDamage() { clear(); }
  void clear() {
    for (int y = 0; y < Height; ++y) { begin_[y] = Width; end_[y] = 0; }
  }
  void mark(int x, int y, int width, int height) {
    for (int row = y; row < y + height; ++row) {
      begin_[row] = std::min<int>(begin_[row], x);
      end_[row] = std::max<int>(end_[row], x + width);
    }
  }
  bool changed() const {
    for (int y = 0; y < Height; ++y) if (end_[y] > begin_[y]) return true;
    return false;
  }
  void synchronize(const uint16_t *published, uint16_t *retired) {
    for (int y = 0; y < Height; ++y) {
      int begin = begin_[y], end = end_[y];
      if (end > begin)
        memcpy(retired + y * Width + begin, published + y * Width + begin, (end - begin) * 2);
    }
    clear();
  }
};
