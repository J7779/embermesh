// EmberMesh store — platform VFS: POSIX (Linux, macOS, Android, iOS, Emscripten) and Win32.
// See store/vfs.h for the durability contract and per-platform notes.
#include "store/vfs.h"

#include <cstring>
#include <limits>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ember::store {
namespace {

// Largest single read/write syscall we issue (keeps 32-bit size types and ssize_t happy).
constexpr size_t kMaxIoChunk = size_t(1) << 30;

#if defined(_WIN32)
// ------------------------------------------------------------------------------------------------
// Win32
// ------------------------------------------------------------------------------------------------

std::wstring widen(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
  return w;
}

std::string narrow(const wchar_t* w) {
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) return std::string();
  std::string s(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
  return s;
}

Status map_error(DWORD e) {
  switch (e) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND: return Status::NotFound;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL: return Status::NoSpace;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS: return Status::Exists;
    default: return Status::IoError;
  }
}

class WinFile : public VfsFile {
 public:
  explicit WinFile(HANDLE h) : h_(h) {}
  ~WinFile() override { CloseHandle(h_); }

  Status read_at(uint64_t offset, uint8_t* dst, size_t n, size_t* got) override {
    *got = 0;
    if (n == 0) return Status::Ok;
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFFu);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
    DWORD want = static_cast<DWORD>(n < kMaxIoChunk ? n : kMaxIoChunk);
    DWORD r = 0;
    if (!ReadFile(h_, dst, want, &r, &ov)) {
      DWORD e = GetLastError();
      if (e == ERROR_HANDLE_EOF) return Status::Ok;
      return map_error(e);
    }
    *got = r;
    return Status::Ok;
  }

  Status write_at(uint64_t offset, ByteView data) override {
    size_t done = 0;
    while (done < data.size()) {
      uint64_t off = offset + done;
      OVERLAPPED ov{};
      ov.Offset = static_cast<DWORD>(off & 0xFFFFFFFFu);
      ov.OffsetHigh = static_cast<DWORD>(off >> 32);
      size_t left = data.size() - done;
      DWORD want = static_cast<DWORD>(left < kMaxIoChunk ? left : kMaxIoChunk);
      DWORD w = 0;
      if (!WriteFile(h_, data.data() + done, want, &w, &ov)) return map_error(GetLastError());
      if (w == 0) return Status::IoError;
      done += w;
    }
    return Status::Ok;
  }

  Status append(ByteView data) override {
    uint64_t sz = 0;
    Status s = size(&sz);
    if (s != Status::Ok) return s;
    return write_at(sz, data);
  }

  Status flush() override { return Status::Ok; }

  Status sync() override { return FlushFileBuffers(h_) ? Status::Ok : map_error(GetLastError()); }

  Status size(uint64_t* out) override {
    LARGE_INTEGER li;
    if (!GetFileSizeEx(h_, &li)) return map_error(GetLastError());
    *out = static_cast<uint64_t>(li.QuadPart);
    return Status::Ok;
  }

  Status truncate(uint64_t size) override {
    if (size > static_cast<uint64_t>(std::numeric_limits<LONGLONG>::max())) return Status::TooLarge;
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(size);
    if (!SetFilePointerEx(h_, li, nullptr, FILE_BEGIN)) return map_error(GetLastError());
    if (!SetEndOfFile(h_)) return map_error(GetLastError());
    return Status::Ok;
  }

 private:
  HANDLE h_;
};

class WinVfs : public Vfs {
 public:
  Status open(const std::string& path, OpenMode mode, std::unique_ptr<VfsFile>* out) override {
    DWORD access = GENERIC_READ;
    DWORD disp = OPEN_EXISTING;
    switch (mode) {
      case OpenMode::ReadOnly: break;
      case OpenMode::ReadWrite: access |= GENERIC_WRITE; break;
      case OpenMode::CreateReadWrite: access |= GENERIC_WRITE; disp = OPEN_ALWAYS; break;
      case OpenMode::CreateTruncate: access |= GENERIC_WRITE; disp = CREATE_ALWAYS; break;
    }
    // FILE_FLAG_OPEN_REPARSE_POINT: open a symlink / junction itself instead of its target, then
    // refuse it (and hard-linked files for writing), so scrub / wipe never write outside the store.
    HANDLE h = CreateFileW(widen(path).c_str(), access,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, disp,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE) return map_error(GetLastError());
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(h, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0 ||
        (mode != OpenMode::ReadOnly && info.nNumberOfLinks > 1)) {
      CloseHandle(h);
      return Status::InvalidArgument;
    }
    out->reset(new WinFile(h));
    return Status::Ok;
  }

  Status rename(const std::string& from, const std::string& to) override {
    if (MoveFileExW(widen(from).c_str(), widen(to).c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      return Status::Ok;
    return map_error(GetLastError());
  }

  Status remove(const std::string& path) override {
    std::wstring w = widen(path);
    if (DeleteFileW(w.c_str())) return Status::Ok;
    DWORD e = GetLastError();
    if (RemoveDirectoryW(w.c_str())) return Status::Ok;
    return map_error(e);
  }

  Status list(const std::string& dir, std::vector<std::string>* names) override {
    names->clear();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(widen(path_join(dir, "*")).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
      DWORD e = GetLastError();
      return e == ERROR_FILE_NOT_FOUND ? Status::Ok : map_error(e);
    }
    do {
      std::string n = narrow(fd.cFileName);
      if (n != "." && n != ".." && !n.empty()) names->push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return Status::Ok;
  }

  Status file_size(const std::string& path, uint64_t* out) override {
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(widen(path).c_str(), GetFileExInfoStandard, &a))
      return map_error(GetLastError());
    *out = (static_cast<uint64_t>(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
    return Status::Ok;
  }

  Status mkdir(const std::string& path) override {
    if (CreateDirectoryW(widen(path).c_str(), nullptr)) return Status::Ok;
    DWORD e = GetLastError();
    return e == ERROR_ALREADY_EXISTS ? Status::Ok : map_error(e);
  }

  Status sync_dir(const std::string&) override { return Status::Ok; }
};

#else
// ------------------------------------------------------------------------------------------------
// POSIX (Linux, macOS, Android, iOS, Emscripten)
// ------------------------------------------------------------------------------------------------

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

Status map_errno(int e) {
  switch (e) {
    case ENOENT:
    case ENOTDIR: return Status::NotFound;
    case ENOSPC:
#ifdef EDQUOT
    case EDQUOT:
#endif
      return Status::NoSpace;
    case EEXIST: return Status::Exists;
    case EFBIG: return Status::TooLarge;
    default: return Status::IoError;
  }
}

bool fits_off_t(uint64_t v) {
  return v <= static_cast<uint64_t>(std::numeric_limits<off_t>::max());
}

class PosixFile : public VfsFile {
 public:
  explicit PosixFile(int fd) : fd_(fd) {}
  ~PosixFile() override { ::close(fd_); }

  Status read_at(uint64_t offset, uint8_t* dst, size_t n, size_t* got) override {
    *got = 0;
    if (n == 0) return Status::Ok;
    if (!fits_off_t(offset)) return Status::TooLarge;
    size_t want = n < kMaxIoChunk ? n : kMaxIoChunk;
    for (;;) {
      ssize_t r = ::pread(fd_, dst, want, static_cast<off_t>(offset));
      if (r < 0 && errno == EINTR) continue;
      if (r < 0) return map_errno(errno);
      *got = static_cast<size_t>(r);
      return Status::Ok;
    }
  }

  Status write_at(uint64_t offset, ByteView data) override {
    size_t done = 0;
    while (done < data.size()) {
      uint64_t off = offset + done;
      if (!fits_off_t(off) || !fits_off_t(off + (data.size() - done))) return Status::TooLarge;
      size_t left = data.size() - done;
      size_t want = left < kMaxIoChunk ? left : kMaxIoChunk;
      ssize_t w = ::pwrite(fd_, data.data() + done, want, static_cast<off_t>(off));
      if (w < 0 && errno == EINTR) continue;
      if (w < 0) return map_errno(errno);
      if (w == 0) return Status::IoError;
      done += static_cast<size_t>(w);
    }
    return Status::Ok;
  }

  Status append(ByteView data) override {
    uint64_t sz = 0;
    Status s = size(&sz);
    if (s != Status::Ok) return s;
    return write_at(sz, data);
  }

  Status flush() override { return Status::Ok; }

  Status sync() override {
#if defined(__EMSCRIPTEN__)
    // MEMFS/IDBFS: persistence happens in FS.syncfs() driven from JS.
    return Status::Ok;
#else
#if defined(__APPLE__) && defined(F_FULLFSYNC)
    // Plain fsync on Apple platforms does not flush the drive's write cache.
    if (::fcntl(fd_, F_FULLFSYNC) == 0) return Status::Ok;
    // Some file systems (e.g. network/FAT) do not support F_FULLFSYNC: fall back to fsync.
#endif
    for (;;) {
      if (::fsync(fd_) == 0) return Status::Ok;
      if (errno == EINTR) continue;
      return map_errno(errno);
    }
#endif
  }

  Status size(uint64_t* out) override {
    struct stat st;
    if (::fstat(fd_, &st) != 0) return map_errno(errno);
    *out = static_cast<uint64_t>(st.st_size);
    return Status::Ok;
  }

  Status truncate(uint64_t size) override {
    if (!fits_off_t(size)) return Status::TooLarge;
    for (;;) {
      if (::ftruncate(fd_, static_cast<off_t>(size)) == 0) return Status::Ok;
      if (errno == EINTR) continue;
      return map_errno(errno);
    }
  }

 private:
  int fd_;
};

class PosixVfs : public Vfs {
 public:
  Status open(const std::string& path, OpenMode mode, std::unique_ptr<VfsFile>* out) override {
    int flags = O_CLOEXEC;
    switch (mode) {
      case OpenMode::ReadOnly: flags |= O_RDONLY; break;
      case OpenMode::ReadWrite: flags |= O_RDWR; break;
      case OpenMode::CreateReadWrite: flags |= O_RDWR | O_CREAT; break;
      case OpenMode::CreateTruncate: flags |= O_RDWR | O_CREAT | O_TRUNC; break;
    }
    // Store files are never symlinks: O_NOFOLLOW makes a planted symlink (e.g. tmp-<g>.ekv ->
    // somewhere else) fail instead of letting truncate / scrub / wipe write through it.
    flags |= O_NOFOLLOW;
    int fd;
    do {
      fd = ::open(path.c_str(), flags | O_NONBLOCK, 0600);  // O_NONBLOCK: never hang on a fifo
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
      int e = errno;
      if (e == ELOOP || e == EMLINK || e == ENXIO) return Status::InvalidArgument;
      return map_errno(e);
    }
    struct stat st;
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
        (mode != OpenMode::ReadOnly && st.st_nlink > 1)) {
      // Directories, devices and fifos are not store files; a hard-linked file shares its bytes
      // with a name outside our control, so we never write (or scrub) it.
      ::close(fd);
      return Status::InvalidArgument;
    }
    int fl = ::fcntl(fd, F_GETFL);
    if (fl >= 0) ::fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
    out->reset(new PosixFile(fd));
    return Status::Ok;
  }

  Status rename(const std::string& from, const std::string& to) override {
    return ::rename(from.c_str(), to.c_str()) == 0 ? Status::Ok : map_errno(errno);
  }

  Status remove(const std::string& path) override {
    if (::unlink(path.c_str()) == 0) return Status::Ok;
    int e = errno;
    if (e == EISDIR || e == EPERM || e == EACCES) {
      if (::rmdir(path.c_str()) == 0) return Status::Ok;
    }
    return map_errno(e);
  }

  Status list(const std::string& dir, std::vector<std::string>* names) override {
    names->clear();
    DIR* d = ::opendir(dir.c_str());
    if (!d) return map_errno(errno);
    while (struct dirent* e = ::readdir(d)) {
      if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
      names->emplace_back(e->d_name);
    }
    ::closedir(d);
    return Status::Ok;
  }

  Status file_size(const std::string& path, uint64_t* out) override {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return map_errno(errno);
    *out = static_cast<uint64_t>(st.st_size);
    return Status::Ok;
  }

  Status mkdir(const std::string& path) override {
    if (::mkdir(path.c_str(), 0700) == 0) return Status::Ok;
    if (errno == EEXIST) {
      struct stat st;
      if (::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return Status::Ok;
      return Status::Exists;
    }
    return map_errno(errno);
  }

  Status sync_dir(const std::string& dir) override {
#if defined(__EMSCRIPTEN__)
    return Status::Ok;
#else
    int fd = ::open(dir.empty() ? "." : dir.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return map_errno(errno);
    int r;
    do {
      r = ::fsync(fd);
    } while (r != 0 && errno == EINTR);
    int e = errno;
    ::close(fd);
    // Some file systems refuse fsync on directories; their metadata is then not orderable by us.
    if (r != 0 && (e == EINVAL || e == ENOTSUP || e == EBADF)) return Status::Ok;
    return r == 0 ? Status::Ok : map_errno(e);
#endif
  }
};
#endif

}  // namespace

std::unique_ptr<Vfs> new_os_vfs() {
#if defined(_WIN32)
  return std::unique_ptr<Vfs>(new WinVfs());
#else
  return std::unique_ptr<Vfs>(new PosixVfs());
#endif
}

}  // namespace ember::store
