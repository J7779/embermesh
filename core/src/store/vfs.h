// EmberMesh store — virtual file system.
//
// `store` is the only core module allowed to touch files, and it does so exclusively through this
// interface. Two implementations ship:
//
//   * new_os_vfs()  — the real thing: POSIX (Linux, macOS, Android, iOS, Emscripten) or Win32.
//   * MemVfs        — in-memory, deterministic, fault-injecting (store/mem_vfs.h). Used by the tests
//                     and usable as a non-persistent backend ("storage_dir": "").
//
// Paths are UTF-8 strings using '/' as separator (Windows accepts '/' too). The store only ever
// uses flat directories: <dir>/<name>, no nesting below the store directory.
//
// Durability contract (what the store relies on, and what MemVfs enforces strictly):
//   * Data written to a file is durable only after VfsFile::sync() returned Ok.
//   * Creating, renaming or removing a directory entry is durable only after sync_dir() on the
//     containing directory returned Ok.
//   * rename(from, to) atomically replaces `to` (if it exists): after a crash either the old or the
//     new file is found under `to`, never a mix.
//   * After a failed sync() the content of the file is unknown (fsync-gate): callers must not
//     assume anything written since the last good sync survived.
//
// Platform notes:
//   * POSIX: pread/pwrite/fsync (F_FULLFSYNC on Apple, which flushes the drive cache), rename(2),
//     directory fsync. Files are created 0600, directories 0700.
//   * Windows (MSVC): CreateFileW/ReadFile/WriteFile with explicit offsets, FlushFileBuffers,
//     MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH) for atomic replace; sync_dir() is a no-op
//     (NTFS journals metadata; WRITE_THROUGH covers the rename). Files must be closed before
//     they are renamed or removed (the store always does this).
//   * Emscripten: the same POSIX calls run against MEMFS; the app mounts an IDBFS directory and
//     calls FS.syncfs() from JS to persist. fsync and directory sync are no-ops there, so the
//     durability point on the web is the JS-side syncfs. IDBFS persists each file independently,
//     so after a tab crash during syncfs the directory may hold an old and a new generation file
//     side by side — the store's generation-numbered files are designed to resolve exactly that.
//     rename is atomic within MEMFS.
//   * 32-bit targets: offsets are uint64_t in this API; the POSIX backend rejects offsets that do
//     not fit the platform off_t (2 GiB on 32-bit Android without large-file support) with
//     Status::TooLarge rather than wrapping.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ember/bytes.h"
#include "store/status.h"

namespace ember::store {

enum class OpenMode : uint8_t {
  ReadOnly,        // existing file, reads only
  ReadWrite,       // existing file, reads and writes
  CreateReadWrite, // create if missing (empty), keep content if present
  CreateTruncate,  // create if missing, truncate to zero length if present
};

// An open file. Not thread-safe (the core is single-threaded). Closing happens in the destructor;
// it does not imply sync().
class VfsFile {
 public:
  virtual ~VfsFile() = default;

  // Reads up to n bytes at `offset` into dst. May return fewer bytes than requested (short read),
  // even before end of file; *got == 0 with Status::Ok means end of file. Use read_exact() for a
  // loop that handles short reads.
  virtual Status read_at(uint64_t offset, uint8_t* dst, size_t n, size_t* got) = 0;

  // Writes all of `data` at `offset`, extending the file (zero-filling any gap) as needed.
  // On failure an unknown prefix of `data` may have been written (torn write).
  virtual Status write_at(uint64_t offset, ByteView data) = 0;

  // write_at(size()). Same torn-write caveat.
  virtual Status append(ByteView data) = 0;

  // Pushes user-space buffers to the OS (no durability guarantee). The shipped backends do not
  // buffer, so this is cheap.
  virtual Status flush() = 0;

  // flush() + make the file's data and size durable on stable storage.
  virtual Status sync() = 0;

  virtual Status size(uint64_t* out) = 0;

  // Shrinks or extends (zero-fill) the file to exactly `size` bytes. Not durable until sync().
  virtual Status truncate(uint64_t size) = 0;
};

class Vfs {
 public:
  virtual ~Vfs() = default;

  virtual Status open(const std::string& path, OpenMode mode, std::unique_ptr<VfsFile>* out) = 0;

  // Atomically renames `from` to `to`, replacing `to` if it exists. Not durable until sync_dir().
  virtual Status rename(const std::string& from, const std::string& to) = 0;

  // Removes a file or an empty directory. NotFound if it does not exist.
  virtual Status remove(const std::string& path) = 0;

  // Names (not paths) of the entries in `dir`, excluding "." and "..", in unspecified order.
  virtual Status list(const std::string& dir, std::vector<std::string>* names) = 0;

  // Size of a file without opening it. NotFound if missing.
  virtual Status file_size(const std::string& path, uint64_t* out) = 0;

  // Creates a directory (parents must exist). Ok if it already exists.
  virtual Status mkdir(const std::string& path) = 0;

  // Makes creations / renames / removals inside `dir` durable.
  virtual Status sync_dir(const std::string& dir) = 0;
};

// The platform file system (see platform notes above). Never null.
std::unique_ptr<Vfs> new_os_vfs();

// Reads exactly n bytes at offset, looping over short reads. Returns Status::Corruption if the
// file ends before n bytes (for the store a short file means truncation), IoError on error.
Status read_exact(VfsFile& f, uint64_t offset, uint8_t* dst, size_t n);

// Reads a whole file (capped at max_bytes; TooLarge if bigger). For small files only.
Status read_file(Vfs& vfs, const std::string& path, Bytes* out, size_t max_bytes);

// "a" + "/" + "b", without doubling a trailing separator. Empty dir yields just `name`.
std::string path_join(std::string_view dir, std::string_view name);

}  // namespace ember::store
