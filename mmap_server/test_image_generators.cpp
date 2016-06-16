#include <cstdint>
#include <cstdlib>

namespace vdx {

void GenerateNoise(int w, int h, uint8_t *buffer) {
  for (size_t i = 0; i < (w * h * sizeof(uint8_t)); ++i)
    buffer[i] = rand();
}

void GenerateCheckerboard(int w, int h, uint8_t color, uint8_t *buffer) {
  int t_border_size = 10;
  int b_border_size = 10;
  int checkered_height = h - t_border_size - b_border_size;
  int v_n_rects = 20;
  int rect_size = checkered_height / v_n_rects;
  b_border_size += checkered_height % v_n_rects;

  int l_border_size = 10;
  int r_border_size = 10;
  int checkered_width = w - l_border_size - r_border_size;
  int h_n_rects = checkered_width / rect_size;
  r_border_size += checkered_width - (h_n_rects * rect_size);

  const auto c = color;
  const auto cinv = ~color;

  uint8_t *ptr = buffer;

  // Top border
  for (int x = 0; x < w; ++x)
    *ptr++ = cinv;

  for (int y = 0; y < t_border_size - 1; ++y) {
    *ptr++ = cinv;
    for (int x = 0; x < w - 2; ++x)
      *ptr++ = c;
    *ptr++ = cinv;
  }

  uint8_t row_color = color;

  // Body, left and right borders
  for (int y = 0; y < v_n_rects; ++y) {
    for (int row = 0; row < rect_size; ++row) {
      // A single scanline
      // Thin left border
      *ptr++ = cinv;
      // Thick left border
      for (int x = 0; x < l_border_size - 1; ++x)
        *ptr++ = c;

      uint8_t col_color = row_color;
      for (int col = 0; col < h_n_rects; ++col) {
        for (int i = 0; i < rect_size; ++i)
          *ptr++ = col_color;
        col_color = ~col_color;
      }

      // Thick right border
      for (int x = 0; x < r_border_size - 1; ++x)
        *ptr++ = c;

      // Thin right border
      *ptr++ = cinv;
    }

    row_color = ~row_color;
  }

  // Bottom border
  for (int y = 0; y < b_border_size - 1; ++y) {
    *ptr++ = cinv;
    for (int x = 0; x < w - 2; ++x)
      *ptr++ = color;
    *ptr++ = cinv;
  }

  for (int x = 0; x < w; ++x)
    *ptr++ = cinv;

  // Origin mark
  ptr = buffer;
  for (int y = 0; y < (rect_size / 2); ++y) {
    for (int x = 0; x < (rect_size / 2); ++x)
      ptr[y * w + x] = cinv;
  }

  // Origin single dot
  *ptr = color;
}

}  // namespace vdx
