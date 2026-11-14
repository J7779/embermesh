// EmberMesh store — in-memory fault-injecting VFS. See store/mem_vfs.h.
#include "store/mem_vfs.h"

#include <algorithm>
#include <cstring>

namespace ember::store {
namespace {

uint64_t splitmix(uint64_t& s) {
  uint64_t z = (s += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

bool is_root(const std::string& d) { return d.empty() || d == "/" || d == "."; }

}  // namespace

class MemFile : public VfsFile {
 public:
  MemFile(MemVfs* vfs, uint32_t ino, uint64_t epoch, bool writable)
      : vfs_(vfs), ino_(ino), epoch_(epoch), writable_(writable) {}

  Status read_at(uint64_t offset, uint8_t* dst, size_t n, size_t* got) override {
    *got = 0;
    if (!valid()) return Status::IoError;
    return vfs_->do_read(ino_, offset, dst, n, got);
  }
  Status write_at(uint64_t offset, ByteView data) override {
    if (!valid() || !writable_) return Status::IoError;
    return vfs_->do_write(ino_, offset, data);
  }
  Status append(ByteView data) override {
    if (!valid() || !writable_) return Status::IoError;
    MemVfs::Inode* in = vfs_->inode(ino_);
    if (!in) return Status::IoError;
    return vfs_->do_write(ino_, in->data.size(), data);
  }
  Status flush() override { return valid() ? Status::Ok : Status::IoError; }
  Status sync() override {
    if (!valid()) return Status::IoError;
    return vfs_->do_sync(ino_);
  }
  Status size(uint64_t* out) override {
    if (!valid() || vfs_->crashed_) return Status::IoError;
    MemVfs::Inode* in = vfs_->inode(ino_);
    if (!in) return Status::IoError;
    *out = in->data.size();
    return Status::Ok;
  }
  Status truncate(uint64_t size) override {
    if (!valid() || !writable_) return Status::IoError;
    return vfs_->do_truncate(ino_, size);
  }

 private:
  bool valid() const { return vfs_->epoch_ == epoch_; }
  MemVfs* vfs_;
  uint32_t ino_;
  uint64_t epoch_;
  bool writable_;
};

void MemVfs::Inode::preserve_from(size_t off) {
  if (off >= clean_len) return;
  Bytes tail(data.begin() + static_cast<std::ptrdiff_t>(off),
             data.begin() + static_cast<std::ptrdiff_t>(clean_len));
  tail.insert(tail.end(), saved_tail.begin(), saved_tail.end());
  saved_tail.swap(tail);
  clean_len = off;
}

Bytes MemVfs::Inode::durable() const {
  Bytes d(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(clean_len));
  d.insert(d.end(), saved_tail.begin(), saved_tail.end());
  return d;
}

MemVfs::MemVfs() = default;
MemVfs::~MemVfs() = default;

std::string MemVfs::parent_of(const std::string& path) {
  size_t p = path.find_last_of('/');
  if (p == std::string::npos) return std::string();
  if (p == 0) return "/";
  return path.substr(0, p);
}

MemVfs::Inode* MemVfs::inode(uint32_t ino) {
  auto it = inodes_.find(ino);
  return it == inodes_.end() ? nullptr : &it->second;
}

bool MemVfs::begin_op() {
  if (crashed_) return false;
  if (crash_ops_budget_ == 0) {
    crashed_ = true;
    return false;
  }
  if (crash_ops_budget_ > 0) --crash_ops_budget_;
  ++ops_;
  return true;
}

void MemVfs::crash_after_ops(int64_t n) { crash_ops_budget_ = n < 0 ? -1 : n; }
void MemVfs::crash_after_bytes(int64_t n) { crash_bytes_budget_ = n < 0 ? -1 : n; }

uint64_t MemVfs::used_bytes() const {
  uint64_t used = 0;
  std::set<uint32_t> seen;
  for (const auto& kv : names_) {
    if (!seen.insert(kv.second).second) continue;
    auto it = inodes_.find(kv.second);
    if (it != inodes_.end()) used += it->second.data.size();
  }
  return used;
}

Status MemVfs::do_write(uint32_t ino, uint64_t off, ByteView data) {
  if (!begin_op()) return Status::IoError;
  Inode* in = inode(ino);
  if (!in) return Status::IoError;
  if (off > (uint64_t(1) << 40)) return Status::TooLarge;
  size_t allowed = data.size();
  bool crash = false, full = false;
  if (crash_bytes_budget_ >= 0 && static_cast<uint64_t>(crash_bytes_budget_) < allowed) {
    allowed = static_cast<size_t>(crash_bytes_budget_);
    crash = true;
  }
  // Disk-full accounting: growth beyond the current file end must fit in the free space.
  uint64_t cur = in->data.size();
  uint64_t used = used_bytes();
  uint64_t free_bytes = capacity_ > used ? capacity_ - used : 0;
  uint64_t max_end = cur + free_bytes;
  if (off + allowed > max_end) {
    allowed = max_end > off ? static_cast<size_t>(max_end - off) : 0;
    full = true;
  }
  if (allowed > 0) {
    size_t o = static_cast<size_t>(off);
    in->preserve_from(o);
    if (in->data.size() < o + allowed) {
      if (in->data.size() < o) in->preserve_from(in->data.size());
      in->data.resize(o + allowed, 0);
    }
    std::memcpy(in->data.data() + o, data.data(), allowed);
  } else if (off > cur && !full && !crash) {
    in->data.resize(static_cast<size_t>(off), 0);
  }
  written_ += allowed;
  if (crash_bytes_budget_ >= 0) crash_bytes_budget_ -= static_cast<int64_t>(allowed);
  if (crash) {
    crashed_ = true;
    return Status::IoError;
  }
  return full ? Status::NoSpace : Status::Ok;
}

Status MemVfs::do_read(uint32_t ino, uint64_t off, uint8_t* dst, size_t n, size_t* got) {
  *got = 0;
  if (crashed_ || fail_reads_) return Status::IoError;
  Inode* in = inode(ino);
  if (!in) return Status::IoError;
  if (off >= in->data.size()) return Status::Ok;
  size_t avail = in->data.size() - static_cast<size_t>(off);
  size_t take = n < avail ? n : avail;
  if (short_read_max_ && take > short_read_max_) take = short_read_max_;
  if (take) std::memcpy(dst, in->data.data() + off, take);
  *got = take;
  return Status::Ok;
}

Status MemVfs::do_sync(uint32_t ino) {
  if (!begin_op()) return Status::IoError;
  if (fail_sync_) return Status::IoError;
  Inode* in = inode(ino);
  if (!in) return Status::IoError;
  in->make_durable();
  return Status::Ok;
}

Status MemVfs::do_truncate(uint32_t ino, uint64_t size) {
  if (!begin_op()) return Status::IoError;
  Inode* in = inode(ino);
  if (!in) return Status::IoError;
  if (size > (uint64_t(1) << 40)) return Status::TooLarge;
  size_t s = static_cast<size_t>(size);
  if (s < in->data.size()) {
    in->preserve_from(s);
    in->data.resize(s);
  } else if (s > in->data.size()) {
    uint64_t used = used_bytes();
    if (used + (s - in->data.size()) > capacity_) return Status::NoSpace;
    in->data.resize(s, 0);
  }
  return Status::Ok;
}

Status MemVfs::open(const std::string& path, OpenMode mode, std::unique_ptr<VfsFile>* out) {
  if (crashed_) return Status::IoError;
  if (dirs_.count(path)) return Status::InvalidArgument;
  std::string parent = parent_of(path);
  if (!is_root(parent) && !dirs_.count(parent)) return Status::NotFound;
  auto it = names_.find(path);
  uint32_t ino;
  if (it == names_.end()) {
    if (mode == OpenMode::ReadOnly || mode == OpenMode::ReadWrite) return Status::NotFound;
    if (!begin_op()) return Status::IoError;
    ino = next_ino_++;
    inodes_[ino];
    names_[path] = ino;
  } else {
    ino = it->second;
    if (mode == OpenMode::CreateTruncate) {
      if (!begin_op()) return Status::IoError;
      Inode* in = inode(ino);
      in->preserve_from(0);
      in->data.clear();
    }
  }
  out->reset(new MemFile(this, ino, epoch_, mode != OpenMode::ReadOnly));
  return Status::Ok;
}

Status MemVfs::rename(const std::string& from, const std::string& to) {
  if (!begin_op()) return Status::IoError;
  auto it = names_.find(from);
  if (it == names_.end()) return Status::NotFound;
  std::string parent = parent_of(to);
  if (!is_root(parent) && !dirs_.count(parent)) return Status::NotFound;
  if (dirs_.count(to)) return Status::InvalidArgument;
  uint32_t ino = it->second;
  names_.erase(it);
  names_[to] = ino;
  return Status::Ok;
}

Status MemVfs::remove(const std::string& path) {
  if (!begin_op()) return Status::IoError;
  auto it = names_.find(path);
  if (it != names_.end()) {
    names_.erase(it);
    return Status::Ok;
  }
  if (dirs_.count(path)) {
    for (const auto& kv : names_)
      if (parent_of(kv.first) == path) return Status::IoError;  // not empty
    for (const auto& d : dirs_)
      if (parent_of(d) == path) return Status::IoError;
    dirs_.erase(path);
    return Status::Ok;
  }
  return Status::NotFound;
}

Status MemVfs::list(const std::string& dir, std::vector<std::string>* names) {
  names->clear();
  if (crashed_) return Status::IoError;
  if (!is_root(dir) && !dirs_.count(dir)) return Status::NotFound;
  std::string norm = dir == "." ? std::string() : dir;
  for (const auto& kv : names_)
    if (parent_of(kv.first) == norm) names->push_back(kv.first.substr(kv.first.find_last_of('/') + 1));
  for (const auto& d : dirs_)
    if (parent_of(d) == norm) names->push_back(d.substr(d.find_last_of('/') + 1));
  return Status::Ok;
}

Status MemVfs::file_size(const std::string& path, uint64_t* out) {
  if (crashed_) return Status::IoError;
  auto it = names_.find(path);
  if (it == names_.end()) return Status::NotFound;
  *out = inodes_[it->second].data.size();
  return Status::Ok;
}

Status MemVfs::mkdir(const std::string& path) {
  if (dirs_.count(path) || is_root(path)) return crashed_ ? Status::IoError : Status::Ok;
  if (names_.count(path)) return Status::Exists;
  std::string parent = parent_of(path);
  if (!is_root(parent) && !dirs_.count(parent)) return Status::NotFound;
  if (!begin_op()) return Status::IoError;
  dirs_.insert(path);
  return Status::Ok;
}

Status MemVfs::sync_dir(const std::string& dir) {
  if (!begin_op()) return Status::IoError;
  if (fail_sync_) return Status::IoError;
  std::string norm = dir == "." ? std::string() : dir;
  for (auto it = durable_names_.begin(); it != durable_names_.end();) {
    if (parent_of(it->first) == norm && !names_.count(it->first))
      it = durable_names_.erase(it);
    else
      ++it;
  }
  for (const auto& kv : names_)
    if (parent_of(kv.first) == norm) durable_names_[kv.first] = kv.second;
  return Status::Ok;
}

void MemVfs::reboot(CrashMode mode, uint64_t seed) {
  uint64_t rng = seed ^ 0xC0FFEEull;
  switch (mode) {
    case CrashMode::DropUnsynced:
      names_ = durable_names_;
      for (auto& kv : inodes_) {
        Inode& in = kv.second;
        in.data = in.durable();
        in.make_durable();
      }
      break;
    case CrashMode::KeepAll:
      durable_names_ = names_;
      for (auto& kv : inodes_) kv.second.make_durable();
      break;
    case CrashMode::Random: {
      if (splitmix(rng) & 1)
        names_ = durable_names_;
      else
        durable_names_ = names_;
      for (auto& kv : inodes_) {
        Inode& in = kv.second;
        if (in.saved_tail.empty()) {
          size_t unsynced = in.data.size() - in.clean_len;
          if (unsynced > 0) {
            uint64_t r = splitmix(rng);
            size_t keep;
            switch (r & 3) {
              case 0: keep = 0; break;
              case 1: keep = unsynced; break;
              default: keep = static_cast<size_t>((r >> 8) % (unsynced + 1)); break;
            }
            in.data.resize(in.clean_len + keep);
            uint64_t j = splitmix(rng);
            if ((j & 3) == 0) {  // torn tail with junk: zeros or random bytes past the real end
              size_t n = 1 + static_cast<size_t>((j >> 8) % 24);
              bool zeros = (j >> 16) & 1;
              for (size_t i = 0; i < n; ++i)
                in.data.push_back(zeros ? 0 : static_cast<uint8_t>(splitmix(rng)));
            }
          }
        } else if (splitmix(rng) & 1) {
          in.data = in.durable();
        }
        in.make_durable();
      }
      break;
    }
  }
  crashed_ = false;
  crash_ops_budget_ = -1;
  crash_bytes_budget_ = -1;
  ++epoch_;
}

bool MemVfs::exists(const std::string& path) const {
  return names_.count(path) != 0 || dirs_.count(path) != 0;
}

Bytes MemVfs::contents(const std::string& path) const {
  auto it = names_.find(path);
  if (it == names_.end()) return Bytes();
  auto in = inodes_.find(it->second);
  return in == inodes_.end() ? Bytes() : in->second.data;
}

void MemVfs::set_contents(const std::string& path, ByteView data) {
  auto it = names_.find(path);
  uint32_t ino;
  if (it == names_.end()) {
    ino = next_ino_++;
    names_[path] = ino;
    durable_names_[path] = ino;
  } else {
    ino = it->second;
  }
  Inode& in = inodes_[ino];
  in.data = data.to_bytes();
  in.make_durable();
}

bool MemVfs::corrupt(const std::string& path, uint64_t offset, uint8_t mask) {
  auto it = names_.find(path);
  if (it == names_.end()) return false;
  Inode& in = inodes_[it->second];
  if (offset >= in.data.size()) return false;
  size_t o = static_cast<size_t>(offset);
  in.data[o] ^= mask;
  if (o < in.clean_len) {
    // durable copy shares the prefix: already flipped.
  } else if (in.saved_tail.size() > o - in.clean_len) {
    in.saved_tail[o - in.clean_len] ^= mask;
  }
  return true;
}

std::vector<std::string> MemVfs::all_files() const {
  std::vector<std::string> out;
  for (const auto& kv : names_) out.push_back(kv.first);
  return out;
}

}  // namespace ember::store
