#include "vdx.h"

#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>

#include <Windows.h>

#define VDX_PROTO


namespace vdx {

namespace {
const static wchar_t kSharedMemoryName[] = L"ArizonaVisionSystemVideoBitmap";
const static wchar_t kReadyEventName[] = L"ArizonaVisionSystemVideoReadyEvent";

const static std::wstring kMappedFileName = { L"azvs_shmem$$$.tmp" };

// Full HD
const int kMaxSupportedImageWidth = 1920;
const int kMaxSupportedImageHeight = 1080;
#ifdef VDX_PROTO
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

template<class Ptr_>
inline Ptr_ ptr_add(Ptr_ p, ptrdiff_t amount) {
  static_assert(std::is_pointer<Ptr_>::value, "Ptr_ must be pointer type");
  return reinterpret_cast<Ptr_>(reinterpret_cast<uint8_t *>(p)+amount);
}

template<class Ptr_>
inline Ptr_ ptr_sub(Ptr_ p, ptrdiff_t amount) {
  static_assert(std::is_pointer<Ptr_>::value, "Ptr_ must be pointer type");
  return reinterpret_cast<Ptr_>(reinterpret_cast<uint8_t *>(p)-amount);
}

template<class R_, class Ptr1_, class Ptr2_>
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

  event_.Set(CreateEvent(NULL, FALSE, FALSE, ready_event_name_.c_str()));
  if (!event_.IsValid())
    goto exit;

  wchar_t temp_path[MAX_PATH + 1];
  DWORD tmp_path_len = GetTempPath(MAX_PATH + 1, temp_path);
  if (tmp_path_len == 0 || tmp_path_len > MAX_PATH)
    goto exit;

  if ((tmp_path_len + kMappedFileName.size()) > MAX_PATH)
    goto exit;

#pragma warning(suppress:4996)
  wcscpy(&temp_path[tmp_path_len], kMappedFileName.c_str());

  file_.Set(CreateFile(temp_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                       nullptr, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                       nullptr));
  if (!file_.IsValid())
    goto exit;

  DWORD mmap_size_h = (shmem_size_ >> 32) & 0xFFFFFFFF;
  DWORD mmap_size_l = shmem_size_ & 0xFFFFFFFF;

  // Increase file size
  SetFilePointer(file_, shmem_size_, NULL, FILE_BEGIN);
  if (SetEndOfFile(file_) == 0)
    goto exit;

  mmap_.Set(CreateFileMapping(file_, nullptr, PAGE_READWRITE | SEC_COMMIT,
                              mmap_size_h, mmap_size_l,
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

#ifdef VDX_PROTO
  // TODO: Temporary here until proper bitmap conversion is implemented.
  if (w > width_ || h > height_)
    return nullptr;
#endif

  BITMAP *bitmap = reinterpret_cast<BITMAP *>(view_);
  bitmap->bmType = 0;
#ifdef VDX_PROTO
  bitmap->bmWidth = w;
  bitmap->bmHeight = h;
  bitmap->bmWidthBytes = stride_;
  bitmap->bmPlanes = 1;
  bitmap->bmBitsPixel = bytes_per_pixel_ * 8;
#else
  bitmap->bmWidth = width_;
  bitmap->bmHeight = height_;
  bitmap->bmWidthBytes = stride_;
  bitmap->bmPlanes = 3;
  bitmap->bmBitsPixel = bytes_per_pixel_ * 8;
#endif

  void *bits = ptr_add(view_, sizeof(BITMAP));
  size_t bits_space = shmem_size_ - sizeof(BITMAP);
  void *aligned =
      std::align(sizeof(std::max_align_t), bits_space, bits, bits_space);
  if (aligned == nullptr)
    return nullptr;

  bitmap->bmBits = ptr_diff<LPVOID>(aligned, bits);

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
