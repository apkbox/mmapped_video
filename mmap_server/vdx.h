#if !defined(JOB_CONTROL_VDX_H__)
#define JOB_CONTROL_VDX_H__

#include <cinttypes>
#include <string>

#include <Windows.h>

namespace vdx {

namespace internal {
// Encapsulates a Windows handle that has
// a scope lifetime. The class correctly handles
// both NULL and INVALID_HANDLE_VALUE handles.
// Note that internally the invalid handle values
// are normalized to NULL.
class ScopedHandle {
public:
  explicit ScopedHandle(HANDLE handle) : handle_(handle) {
    if (handle_ == INVALID_HANDLE_VALUE)
      handle_ = NULL;
  }
  ScopedHandle() : handle_(NULL) {}
  ~ScopedHandle() {
    try {
      Close();
    } catch (...) {}
  }

  // Checks whether the handle is valid, that is
  // not NULL or INVALID_HANDLE_VALUE.
  // The wrapper can also be compared with NULL by value to
  // determine whether the handle is valid.
  bool IsValid() const { return handle_ != NULL; }

  // Explicitly returns an underlying handle.
  HANDLE Get() const { return handle_; }

  // Implicitly returns and underlying handle.
  operator HANDLE() const { return handle_; }

  // Revokes an ownership of a handle and returns it.
  HANDLE Take() {
    HANDLE tmp = handle_;
    handle_ = NULL;
    return tmp;
  }

  // Explicitly sets the handle. If wrapper already owns
  // a handle, the method closes the currently owned
  // handle first. Note that if original handle was
  // INVALID_HANDLE_VALUE, then it is normalized to NULL.
  void Set(HANDLE handle) {
    Close();
    handle_ = handle;
    if (handle_ == INVALID_HANDLE_VALUE)
      handle_ = NULL;
  }

  // Closes the underlying handle. After calling this method
  // the underlying handle is NULL.
  void Close() {
    if (handle_ != NULL)
      CloseHandle(handle_);
    handle_ = NULL;
  }

  HANDLE operator=(const HANDLE &handle) = delete;
  ScopedHandle(HANDLE &&handle) = delete;
  HANDLE &operator=(HANDLE &&handle) = delete;

private:
  HANDLE handle_;
};

}  // namespace internal

// Facilitates sharing captured images across processes.
// This class is not thread-safe.
// The shared memory is as following:
// Offset         Data          Size
// ---------------------------------------------------------
//  +0            BITMAP        32 bytes (x64) (28 on x86)
//  +20           Padding
//  +bmBits       Bitmap bits   bmWidthBytes * bmHeight
//
// Thus bmBits contains offset to bitmap bits from the start
// of shared memory region.
class VDX {
public:
  VDX();
  ~VDX();

  VDX(const VDX &) = delete;
  VDX &operator=(const VDX &) = delete;

  const std::wstring &GetSharedMemoryName() const;
  const std::wstring &GetReadyEventName() const;

  int width() const { return width_; }
  int height() const { return height_; }
  size_t shared_memory_size() const { return shmem_size_; }
  void *shared_memory_ptr() { return view_; }

  bool is_open() const { return view_ != nullptr; }

  // Initializes VDX with specified image dimensions.
  // Returns true if initalization succeeded and the shared
  // memory allocated. The ready event and shared memory
  // names can be retrieved using GetSharedMemoryName and
  // GetReadyEventName methods.
  bool Open(int width, int height);
  void Close();
  void *WriteImageHeader(int format, int w, int h);
  void SignalReady();
  bool WriteImage(int format, int w, int h, const void *data);

private:
  std::wstring shared_memory_name_;
  std::wstring ready_event_name_;

  int width_;
  int height_;
  int bytes_per_pixel_;  // Bytes per pixel
  int stride_;           // Number of bytes per row
  size_t shmem_size_;    // Shared memory size

  internal::ScopedHandle event_;
  internal::ScopedHandle file_;
  internal::ScopedHandle mmap_;
  void *view_;
};

}  // namespace vdx

#endif  // JOB_CONTROL_VDX_H__
