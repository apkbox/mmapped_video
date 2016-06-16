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

namespace vdx {
void GenerateNoise(int w, int h, uint8_t *buffer);
void GenerateCheckerboard(int w, int h, uint8_t color, uint8_t *buffer);
}  // namespace vdx

int main() {
  int image_width = 800;
  int image_height = 600;

  SetConsoleCtrlHandler(HandlerRoutine, TRUE);

  vdx::VDX vdx;
  vdx.Open(image_width, image_height);

  while (!g_stop) {
    std::cout << '.';

    uint8_t *ptr =
        (uint8_t *)vdx.WriteImageHeader(1, image_width, image_height);
#if 1
    static uint8_t color = 0xff;
    vdx::GenerateCheckerboard(image_width, image_height, color, ptr);
    color++;
#else
    vdx::GenerateNoise(image_width, image_height, ptr);
#endif

    vdx.SignalReady();
    Sleep(40);
  }

  vdx.Close();

  SetConsoleCtrlHandler(HandlerRoutine, FALSE);

  return 0;
}