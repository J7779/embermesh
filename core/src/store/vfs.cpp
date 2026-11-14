// EmberMesh store — VFS helpers and status names.
#include "store/vfs.h"

namespace ember::store {

const char* status_name(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NotFound: return "not_found";
    case Status::Exists: return "exists";
    case Status::IoError: return "io_error";
    case Status::NoSpace: return "no_space";
    case Status::Corruption: return "corruption";
    case Status::Locked: return "locked";
    case Status::WrongKey: return "wrong_key";
    case Status::InvalidArgument: return "invalid_argument";
    case Status::TooLarge: return "too_large";
    case Status::ReadOnly: return "read_only";
    case Status::Busy: return "busy";
    case Status::Broken: return "broken";
    case Status::Tampered: return "tampered";
  }
  return "unknown";
}

Status read_exact(VfsFile& f, uint64_t offset, uint8_t* dst, size_t n) {
  size_t done = 0;
  while (done < n) {
    size_t got = 0;
    Status s = f.read_at(offset + done, dst + done, n - done, &got);
    if (s != Status::Ok) return s;
    if (got == 0) return Status::Corruption;  // unexpected end of file
    if (got > n - done) return Status::IoError;  // misbehaving backend
    done += got;
  }
  return Status::Ok;
}

Status read_file(Vfs& vfs, const std::string& path, Bytes* out, size_t max_bytes) {
  out->clear();
  std::unique_ptr<VfsFile> f;
  Status s = vfs.open(path, OpenMode::ReadOnly, &f);
  if (s != Status::Ok) return s;
  uint64_t sz = 0;
  s = f->size(&sz);
  if (s != Status::Ok) return s;
  if (sz > max_bytes) return Status::TooLarge;
  out->resize(static_cast<size_t>(sz));
  s = read_exact(*f, 0, out->data(), out->size());
  if (s != Status::Ok) out->clear();
  return s;
}

std::string path_join(std::string_view dir, std::string_view name) {
  std::string out(dir);
  if (!out.empty() && out.back() != '/' && out.back() != '\\') out.push_back('/');
  out.append(name.data(), name.size());
  return out;
}

}  // namespace ember::store
