#include "vdx.h"

#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

#include <Windows.h>

// min/max macro defined in Windows.h and can be undefined
// by defining NOMINMAX before including Windows.h.
// However, because Windows.h also included into vdx.h, this would
// not have an effect here. So, we just undefine these here
// if we see them.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#define VDX_USE_GRAYSCALE_BITMAP
// #define MMAP_TO_FILE

namespace vdx {

namespace {
const static wchar_t kSharedMemoryName[] = L"ArizonaVisionSystemVideoBitmap";
const static wchar_t kReadyEventName[] = L"ArizonaVisionSystemVideoReadyEvent";

const static std::wstring kMappedFileName = { L"azvs_shmem$$$.tmp" };

// Full HD
const int kMaxSupportedImageWidth = 1920;
const int kMaxSupportedImageHeight = 1080;
#ifdef VDX_USE_GRAYSCALE_BITMAP
const int kDefaultBytesPerPixel = 1;  // 8 bit grayscale
#else
const int kDefaultBytesPerPixel = 3;  // 8 bit RGB
#endif

// Pads number_of_bytes to 32-bit boundary.
template <class T>
inline T Pad32(T n) {
  T padding = n % 4;
  return n + (padding > 0 ? 4 - padding : 0);
}

// Converts number of pixels to number of bytes aligned to the 32-bit boundary.
// bpp - bytes per pixel.
template <class T>
inline T GetStride(T width, int bpp) {
  return Pad32(width * bpp);
}

template <class Ptr_>
inline Ptr_ ptr_add(Ptr_ p, ptrdiff_t amount) {
  static_assert(std::is_pointer<Ptr_>::value, "Ptr_ must be pointer type");
  return reinterpret_cast<Ptr_>(reinterpret_cast<uint8_t *>(p) + amount);
}

template <class Ptr_>
inline Ptr_ ptr_sub(Ptr_ p, ptrdiff_t amount) {
  static_assert(std::is_pointer<Ptr_>::value, "Ptr_ must be pointer type");
  return reinterpret_cast<Ptr_>(reinterpret_cast<uint8_t *>(p) - amount);
}

template <class R_, class Ptr1_, class Ptr2_>
inline R_ ptr_diff(Ptr1_ p1, Ptr2_ p2) {
  static_assert(std::is_pointer<Ptr1_>::value, "Ptr1_ must be pointer type");
  static_assert(std::is_pointer<Ptr2_>::value, "Ptr2_ must be pointer type");
  return reinterpret_cast<R_>(reinterpret_cast<uint8_t *>(p1) -
                              reinterpret_cast<uint8_t *>(p2));
}

}  // namespace

VDX::VDX()
    : shared_memory_name_(kSharedMemoryName),
      ready_event_name_(kReadyEventName),
      width_(0),
      height_(0),
      bytes_per_pixel_(0),
      stride_(0),
      shmem_size_(0),
      view_(nullptr) {}

VDX::~VDX() {
  try {
    Close();
  } catch (...) {}
}

bool VDX::Open(int width, int height) {
  // Check if already started.
  if (view_ != nullptr)
    return false;

  if (width < 0 || height < 0)
    return false;

  if (width > kMaxSupportedImageWidth || height > kMaxSupportedImageHeight)
    return false;

  bool result = false;

  width_ = width;
  height_ = height;
  bytes_per_pixel_ = kDefaultBytesPerPixel;
  stride_ = GetStride(width, bytes_per_pixel_);

  // The shared memory size includes space for the bitmap header,
  // bitmap data and extra space for alignment.
  // The bitmap data in the memory is platorm aligned.
  // The bmBits member of BITMAP specifies an offset to bitmap data from
  // shared memory base.
  // The alignnment allows efficiently use SSE or other methods that work
  // faster with aligned data.
  shmem_size_ = sizeof(BITMAP) + sizeof(std::max_align_t) + stride_ * height_;

  // 2Gb ummm... is a bit too much for a humble image.
  // So the size is limited to 32 bit (signed).
  assert(shmem_size_ <= std::numeric_limits<int32_t>::max());
  if (shmem_size_ > std::numeric_limits<int32_t>::max())
    goto exit;

  event_.Set(CreateEvent(nullptr, FALSE, FALSE, ready_event_name_.c_str()));
  if (!event_.IsValid())
    goto exit;

  HANDLE file_handle = INVALID_HANDLE_VALUE;

#if defined(MMAP_TO_FILE)
  wchar_t temp_path[MAX_PATH + 1];
  DWORD tmp_path_len = GetTempPath(MAX_PATH + 1, temp_path);
  if (tmp_path_len == 0 || tmp_path_len > MAX_PATH)
    goto exit;

  if ((tmp_path_len + kMappedFileName.size()) > MAX_PATH)
    goto exit;

#pragma warning(suppress : 4996)
  wcscpy(&temp_path[tmp_path_len], kMappedFileName.c_str());

  file_.Set(CreateFile(temp_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                       nullptr, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, /*| FILE_FLAG_DELETE_ON_CLOSE,*/
                       nullptr));
  if (!file_.IsValid())
    goto exit;

  // Increase file size
  SetFilePointer(file_, static_cast<int32_t>(shmem_size_), nullptr, FILE_BEGIN);
  if (SetEndOfFile(file_) == 0)
    goto exit;

  file_handle = file_.Get();
#endif

  mmap_.Set(CreateFileMapping(file_handle, nullptr, PAGE_READWRITE | SEC_COMMIT,
                              0, static_cast<int32_t>(shmem_size_),
                              shared_memory_name_.c_str()));
  if (!mmap_.IsValid())
    goto exit;

  view_ = MapViewOfFile(mmap_, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
  if (view_ == nullptr)
    goto exit;

  result = true;

exit:
  if (!result)
    Close();

  return result;
}

void VDX::Close() {
  event_.Close();

  if (view_ != nullptr)
    UnmapViewOfFile(view_);
  view_ = nullptr;

  mmap_.Close();
  file_.Close();
}

void VDX::SignalReady() {
  if (event_.IsValid())
    SetEvent(event_);
}

void *VDX::WriteImageHeader(int format, int w, int h) {
  if (view_ == nullptr)
    return nullptr;

  if (w > width_ || h > height_)
    return nullptr;

  assert(width_ % 2 == 0);

  BITMAP *bitmap = reinterpret_cast<BITMAP *>(view_);
  bitmap->bmType = 0;
  bitmap->bmWidth = width_;
  bitmap->bmHeight = height_;
  bitmap->bmWidthBytes = stride_;
#if defined(VDX_USE_GRAYSCALE_BITMAP)
  bitmap->bmPlanes = 1;
#else
  bitmap->bmPlanes = 3;
#endif
  bitmap->bmBitsPixel = bytes_per_pixel_ * 8;

  void *bits = ptr_add(view_, sizeof(BITMAP));
  size_t bits_space = shmem_size_ - sizeof(BITMAP);
  void *aligned =
      std::align(sizeof(std::max_align_t), bits_space, bits, bits_space);
  if (aligned == nullptr)
    return nullptr;

  bitmap->bmBits = ptr_diff<LPVOID>(aligned, view_);

  return aligned;
}

bool VDX::WriteImage(int format, int w, int h, const void *data) {
  if (view_ == nullptr)
    return false;

  void *ptr = WriteImageHeader(format, w, h);
  if (ptr == nullptr)
    return false;

  // TODO: Do real conversion.
  memcpy(ptr, data, w * h);
  SignalReady();

  return true;
}

}  // namespace vdx
