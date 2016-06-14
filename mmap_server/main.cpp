#include <cstdint>
#include <cstdlib>
#include <iostream>

#include <Windows.h>

struct ImageHeader {
  uint32_t width;
  uint32_t height;
};

bool g_stop = false;

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType) {
  g_stop = true;
  return TRUE;
}


int main() {
  int image_width = 1280;
  int image_height = 960;

  SetConsoleCtrlHandler(HandlerRoutine, TRUE);

  size_t mmap_size = sizeof(ImageHeader) + image_width * image_height * sizeof(uint8_t);

  char temp_path[MAX_PATH + 1];
  if (GetTempPath(MAX_PATH + 1, temp_path) > (MAX_PATH + 1)) {
    std::cout << "error: GetTempPath failed" << std::endl;
    return 1;
  }

#pragma warning(disable:4996)
  strcat(temp_path, "mmap$$$.tmp");

  HANDLE file =
      CreateFile(temp_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                 nullptr, CREATE_ALWAYS,
                 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    std::cout << "error: CreateFile failed" << std::endl;
    return 1;
  }

  SetFilePointer(file, mmap_size, 0, FILE_BEGIN);

  HANDLE mmap =
      CreateFileMapping(file, nullptr, PAGE_READWRITE, HIWORD(mmap_size),
                        LOWORD(mmap_size), "mmap_video_data");
  if (mmap == NULL) {
    std::cout << "error: CreateFileMapping failed" << std::endl;
    return 1;
  }

  uint8_t *view = (uint8_t *)MapViewOfFile(mmap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
  if (view == nullptr) {
    std::cout << "error: MapViewOfFile failed" << std::endl;
    return 1;
  }

  HANDLE event = CreateEvent(nullptr, false, false, "mmap_video_event");
  if (event == NULL) {
    std::cout << "error: CreateEvent failed" << std::endl;
    return 1;
  }

  while (!g_stop) {
    std::cout << '.';
    ImageHeader *hdr = reinterpret_cast<ImageHeader *>(view);
    hdr->width = image_width;
    hdr->height = image_height;
    uint8_t *bmp_bits = view + sizeof(ImageHeader);

    for (size_t i = 0; i < (hdr->width * hdr->height * sizeof(uint8_t)); ++i) {
      bmp_bits[i] = rand();
    }

    SetEvent(event);
    Sleep(40);
  }

  UnmapViewOfFile(view);
  CloseHandle(mmap);
  CloseHandle(file);
  CloseHandle(event);

  SetConsoleCtrlHandler(HandlerRoutine, FALSE);

  return 0;
}