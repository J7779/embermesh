// EmberMesh store — status codes shared by the VFS layer and the key/value store.
//
// The core is built without exceptions: every fallible store/VFS call returns a Status.
#pragma once

#include <cstdint>

namespace ember::store {

enum class Status : uint8_t {
  Ok = 0,
  NotFound,         // key / file / directory does not exist
  Exists,           // open(): store exists and Options::error_if_exists was set
  IoError,          // the storage device reported an error (or the fault VFS "crashed")
  NoSpace,          // device full (ENOSPC / quota / MemVfs capacity)
  Corruption,       // data failed verification (checksum, authentication or structure)
  Locked,           // store is encrypted and no cipher was supplied
  WrongKey,         // store is encrypted and the supplied cipher cannot open it
  InvalidArgument,  // caller error (bad name, cipher without entropy, ...)
  TooLarge,         // key / value / batch / namespace name over the configured limit
  ReadOnly,         // write attempted on a store opened read-only
  Busy,             // write attempted from inside a scan visitor
  Broken,           // an earlier I/O failure left the on-disk state uncertain; reopen required
  Tampered,         // the directory contradicts the expected encryption (Options::require_encryption):
                    // only plaintext generations, or no store at all, where an encrypted one was expected
};

// Stable lowercase name, e.g. "no_space". Never returns null.
const char* status_name(Status s);

inline bool is_ok(Status s) { return s == Status::Ok; }

}  // namespace ember::store
