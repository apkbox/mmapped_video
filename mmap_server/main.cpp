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


int main() {
  int image_width = 1280;
  int image_height = 960;

  SetConsoleCtrlHandler(HandlerRoutine, TRUE);

  vdx::VDX vdx;
  vdx.Open(image_width, image_height);

  while (!g_stop) {
    std::cout << '.';

    uint8_t *ptr = (uint8_t *)vdx.WriteImageHeader(1, image_width, image_height);

    for (size_t i = 0; i < (image_width * image_height * sizeof(uint8_t)); ++i) {
      ptr[i] = rand();
    }

    vdx.SignalReady();
    Sleep(40);
  }

  vdx.Close();

  SetConsoleCtrlHandler(HandlerRoutine, FALSE);

  return 0;
}