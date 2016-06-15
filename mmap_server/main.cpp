#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>

#include <Windows.h>

#include "vdx.h"

bool g_stop = false;

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType) {
  g_stop = true;
  return TRUE;
}


static void GenerateTestImage(int w, int h, uint8_t color, uint8_t *buffer) {
  int t_border_size = 10;
  int b_border_size = 10;
  int checkered_height = h - t_border_size - b_border_size;
  int v_n_rects = 10;
  int rect_size = checkered_height / v_n_rects;
  b_border_size += checkered_height % v_n_rects;

  int l_border_size = 10;
  int r_border_size = 10;
  int checkered_width = w - l_border_size - r_border_size;
  int h_n_rects = checkered_width / rect_size;
  r_border_size += checkered_width - (h_n_rects * rect_size);

  uint8_t *ptr = buffer;

  // Top border
  for (int y = 0; y < t_border_size; ++y) {
    for (int x = 0; x < w; ++x)
      *ptr++ = color;
  }

  uint8_t row_color = color;

  // Body, left and right borders
  for (int y = 0; y < v_n_rects; ++y) {
    for (int row = 0; row < rect_size; ++row) {
      // Single scanline
      for (int x = 0; x < l_border_size; ++x)
        *ptr++ = color;

      uint8_t col_color = row_color;
      for (int col = 0; col < h_n_rects; ++col) {
        for (int i = 0; i < rect_size; ++i)
          *ptr++ = col_color;
        col_color = ~col_color;
      }
      for (int x = 0; x < r_border_size; ++x)
        *ptr++ = color;
    }

    row_color = ~row_color;
  }

  // Bottom border
  for (int y = 0; y < b_border_size; ++y) {
    for (int x = 0; x < w; ++x)
      *ptr++ = color;
  }
}


int main() {
  int image_width = 1280;
  int image_height = 960;

  SetConsoleCtrlHandler(HandlerRoutine, TRUE);

  vdx::VDX vdx;
  vdx.Open(image_width, image_height);

  while (!g_stop) {
    std::cout << '.';

    uint8_t *ptr = (uint8_t *)vdx.WriteImageHeader(1, image_width, image_height);
#if 1
    static uint8_t color = 0xff;
    GenerateTestImage(image_width, image_height, color, ptr);
    color = ~color;
#else
    for (size_t i = 0; i < (image_width * image_height * sizeof(uint8_t)); ++i) {
      ptr[i] = rand();
    }
#endif

    vdx.SignalReady();
    Sleep(40);
  }

  vdx.Close();

  SetConsoleCtrlHandler(HandlerRoutine, FALSE);

  return 0;
}