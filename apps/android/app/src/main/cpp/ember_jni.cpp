// JNI binding for the EmberMesh C API (core/include/ember/ember.h).
//
// Kotlin side: org.embermesh.app.core.EmberNative. Every function is a thin wrapper; the native
// handle is a pointer to EmberHandle (node + optional memory-mapped guides.pack) cast to jlong.
// All ember_* functions are thread-safe; the Kotlin wrapper (EmberNode) guarantees the handle is not
// destroyed while another thread is inside a call.
#include <jni.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <new>

#include "ember/ember.h"

#define LOG_TAG "EmberJNI"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace {

struct GuidesMapping {
  AAsset* asset = nullptr;  // asset kept open (its buffer is mmapped when stored uncompressed)
  void* map = nullptr;      // or a file mmap
  size_t map_len = 0;

  void release() {
    if (asset) AAsset_close(asset);
    if (map) munmap(map, map_len);
    asset = nullptr;
    map = nullptr;
    map_len = 0;
  }
};

struct EmberHandle {
  ember_node* node = nullptr;
  GuidesMapping guides;
};

inline EmberHandle* H(jlong h) { return reinterpret_cast<EmberHandle*>(static_cast<intptr_t>(h)); }
inline ember_node* N(jlong h) {
  EmberHandle* e = H(h);
  return e ? e->node : nullptr;
}

// Java strings are modified UTF-8; ember wants real UTF-8 (supplementary characters such as emoji
// differ). Convert explicitly through UTF-16.
char* to_utf8(JNIEnv* env, jstring s) {
  if (!s) return nullptr;
  const jsize n = env->GetStringLength(s);
  const jchar* u = env->GetStringChars(s, nullptr);
  if (!u) return nullptr;
  char* out = static_cast<char*>(malloc(static_cast<size_t>(n) * 3 + 1));
  if (!out) {
    env->ReleaseStringChars(s, u);
    return nullptr;
  }
  size_t o = 0;
  for (jsize i = 0; i < n; ++i) {
    uint32_t c = u[i];
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < n && u[i + 1] >= 0xDC00 && u[i + 1] <= 0xDFFF) {
      c = 0x10000 + ((c - 0xD800) << 10) + (u[i + 1] - 0xDC00);
      ++i;
    } else if (c >= 0xD800 && c <= 0xDFFF) {
      c = 0xFFFD;  // lone surrogate
    }
    if (c < 0x80) {
      out[o++] = static_cast<char>(c);
    } else if (c < 0x800) {
      out[o++] = static_cast<char>(0xC0 | (c >> 6));
      out[o++] = static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
      out[o++] = static_cast<char>(0xE0 | (c >> 12));
      out[o++] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      out[o++] = static_cast<char>(0x80 | (c & 0x3F));
    } else {
      out[o++] = static_cast<char>(0xF0 | (c >> 18));
      out[o++] = static_cast<char>(0x80 | ((c >> 12) & 0x3F));
      out[o++] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      out[o++] = static_cast<char>(0x80 | (c & 0x3F));
    }
  }
  out[o] = 0;
  env->ReleaseStringChars(s, u);
  return out;
}

// UTF-8 -> Java string via UTF-16 (tolerates invalid UTF-8 by emitting U+FFFD).
jstring from_utf8(JNIEnv* env, const char* s) {
  if (!s) return nullptr;
  const size_t len = strlen(s);
  jchar* buf = static_cast<jchar*>(malloc((len + 1) * sizeof(jchar) * 2));
  if (!buf) return nullptr;
  size_t o = 0;
  const auto* p = reinterpret_cast<const unsigned char*>(s);
  size_t i = 0;
  while (i < len) {
    uint32_t c = p[i];
    size_t extra = 0;
    if (c < 0x80) {
      extra = 0;
    } else if ((c & 0xE0) == 0xC0) {
      c &= 0x1F;
      extra = 1;
    } else if ((c & 0xF0) == 0xE0) {
      c &= 0x0F;
      extra = 2;
    } else if ((c & 0xF8) == 0xF0) {
      c &= 0x07;
      extra = 3;
    } else {
      buf[o++] = 0xFFFD;
      ++i;
      continue;
    }
    bool ok = true;
    for (size_t k = 1; k <= extra; ++k) {
      if (i + k >= len || (p[i + k] & 0xC0) != 0x80) {
        ok = false;
        break;
      }
      c = (c << 6) | (p[i + k] & 0x3F);
    }
    if (!ok) {
      buf[o++] = 0xFFFD;
      ++i;
      continue;
    }
    i += extra + 1;
    if (c >= 0x10000 && c <= 0x10FFFF) {
      c -= 0x10000;
      buf[o++] = static_cast<jchar>(0xD800 + (c >> 10));
      buf[o++] = static_cast<jchar>(0xDC00 + (c & 0x3FF));
    } else if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) {
      buf[o++] = 0xFFFD;
    } else {
      buf[o++] = static_cast<jchar>(c);
    }
  }
  jstring js = env->NewString(buf, static_cast<jsize>(o));
  free(buf);
  return js;
}

}  // namespace

extern "C" {

JNIEXPORT jstring JNICALL Java_org_embermesh_app_core_EmberNative_version(JNIEnv* env, jclass) {
  return from_utf8(env, ember_version());
}

JNIEXPORT jlong JNICALL Java_org_embermesh_app_core_EmberNative_create(JNIEnv* env, jclass, jstring config,
                                                                      jbyteArray entropy) {
  if (!entropy) return 0;
  const jsize n = env->GetArrayLength(entropy);
  jbyte* e = env->GetByteArrayElements(entropy, nullptr);
  if (!e) return 0;
  char* cfg = to_utf8(env, config);
  ember_node* node = ember_create(cfg ? cfg : "{}", reinterpret_cast<const uint8_t*>(e), static_cast<size_t>(n));
  free(cfg);
  memset(e, 0, static_cast<size_t>(n));
  env->ReleaseByteArrayElements(entropy, e, 0);  // copy the zeroed buffer back
  if (!node) return 0;
  auto* h = new (std::nothrow) EmberHandle();
  if (!h) {
    ember_destroy(node);
    return 0;
  }
  h->node = node;
  return static_cast<jlong>(reinterpret_cast<intptr_t>(h));
}

JNIEXPORT void JNICALL Java_org_embermesh_app_core_EmberNative_destroy(JNIEnv*, jclass, jlong handle) {
  EmberHandle* h = H(handle);
  if (!h) return;
  ember_destroy(h->node);  // first: the core may reference the mapped pack until now
  h->guides.release();
  delete h;
}

JNIEXPORT void JNICALL Java_org_embermesh_app_core_EmberNative_addEntropy(JNIEnv* env, jclass, jlong handle,
                                                                         jbyteArray data) {
  ember_node* node = N(handle);
  if (!node || !data) return;
  const jsize n = env->GetArrayLength(data);
  jbyte* d = env->GetByteArrayElements(data, nullptr);
  if (!d) return;
  ember_add_entropy(node, reinterpret_cast<const uint8_t*>(d), static_cast<size_t>(n));
  env->ReleaseByteArrayElements(data, d, JNI_ABORT);
}

JNIEXPORT jstring JNICALL Java_org_embermesh_app_core_EmberNative_call(JNIEnv* env, jclass, jlong handle,
                                                                      jstring method, jstring params) {
  ember_node* node = N(handle);
  if (!node) return from_utf8(env, R"({"ok":false,"error":{"code":"internal","message":"node not running"}})");
  char* m = to_utf8(env, method);
  char* p = to_utf8(env, params);
  char* r = ember_call(node, m ? m : "", p ? p : "{}");
  free(m);
  free(p);
  if (!r) return from_utf8(env, R"({"ok":false,"error":{"code":"internal","message":"out of memory"}})");
  jstring out = from_utf8(env, r);
  ember_free(r);
  return out;
}

JNIEXPORT void JNICALL Java_org_embermesh_app_core_EmberNative_tick(JNIEnv*, jclass, jlong handle, jlong now_ms) {
  ember_node* node = N(handle);
  if (node) ember_tick(node, static_cast<uint64_t>(now_ms));
}

JNIEXPORT jint JNICALL Java_org_embermesh_app_core_EmberNative_nextWakeupMs(JNIEnv*, jclass, jlong handle) {
  ember_node* node = N(handle);
  return node ? static_cast<jint>(ember_next_wakeup_ms(node)) : 60000;
}

JNIEXPORT void JNICALL Java_org_embermesh_app_core_EmberNative_linkRx(JNIEnv* env, jclass, jlong handle, jint link_id,
                                                                     jbyteArray data, jint off, jint len) {
  ember_node* node = N(handle);
  if (!node || !data || off < 0 || len < 0) return;
  const jsize n = env->GetArrayLength(data);
  if (static_cast<int64_t>(off) + len > n) return;
  jbyte* d = static_cast<jbyte*>(env->GetPrimitiveArrayCritical(data, nullptr));
  if (!d) return;
  // Copy out of the critical region: ember_link_rx takes the node lock and must not run while GC is held off
  // for an unbounded time.
  uint8_t stack_buf[2048];
  uint8_t* buf = len <= static_cast<jint>(sizeof(stack_buf)) ? stack_buf : static_cast<uint8_t*>(malloc(static_cast<size_t>(len)));
  if (buf) memcpy(buf, d + off, static_cast<size_t>(len));
  env->ReleasePrimitiveArrayCritical(data, d, JNI_ABORT);
  if (!buf) return;
  ember_link_rx(node, static_cast<uint32_t>(link_id), buf, static_cast<size_t>(len));
  if (buf != stack_buf) free(buf);
}

JNIEXPORT void JNICALL Java_org_embermesh_app_core_EmberNative_audioRx(JNIEnv* env, jclass, jlong handle, jint link_id,
                                                                      jshortArray samples, jint count, jint rate) {
  ember_node* node = N(handle);
  if (!node || !samples || count < 0 || rate <= 0) return;
  const jsize n = env->GetArrayLength(samples);
  if (count > n) count = n;
  jshort* s = env->GetShortArrayElements(samples, nullptr);
  if (!s) return;
  ember_audio_rx(node, static_cast<uint32_t>(link_id), reinterpret_cast<const int16_t*>(s), static_cast<size_t>(count),
                 static_cast<uint32_t>(rate));
  env->ReleaseShortArrayElements(samples, s, JNI_ABORT);
}

JNIEXPORT jint JNICALL Java_org_embermesh_app_core_EmberNative_audioTx(JNIEnv* env, jclass, jlong handle, jint link_id,
                                                                      jshortArray out, jint max, jint rate) {
  ember_node* node = N(handle);
  if (!node || !out || max <= 0 || rate <= 0) return 0;
  const jsize n = env->GetArrayLength(out);
  if (max > n) max = n;
  jshort* s = env->GetShortArrayElements(out, nullptr);
  if (!s) return 0;
  size_t w = ember_audio_tx(node, static_cast<uint32_t>(link_id), reinterpret_cast<int16_t*>(s), static_cast<size_t>(max),
                            static_cast<uint32_t>(rate));
  if (w > static_cast<size_t>(max)) w = static_cast<size_t>(max);
  env->ReleaseShortArrayElements(out, s, 0);
  return static_cast<jint>(w);
}

// Returns null when the outbox is empty, else [kind:int32 LE][link_id:uint32 LE][payload...].
JNIEXPORT jbyteArray JNICALL Java_org_embermesh_app_core_EmberNative_poll(JNIEnv* env, jclass, jlong handle) {
  ember_node* node = N(handle);
  if (!node) return nullptr;
  ember_out out{};
  if (!ember_poll(node, &out)) return nullptr;
  const size_t len = out.data ? out.len : 0;
  if (len > 0x7FFFFFF0) return nullptr;
  jbyteArray arr = env->NewByteArray(static_cast<jsize>(len + 8));
  if (!arr) return nullptr;
  uint8_t hdr[8];
  const uint32_t k = static_cast<uint32_t>(out.kind);
  for (int i = 0; i < 4; ++i) {
    hdr[i] = static_cast<uint8_t>(k >> (8 * i));
    hdr[4 + i] = static_cast<uint8_t>(out.link_id >> (8 * i));
  }
  env->SetByteArrayRegion(arr, 0, 8, reinterpret_cast<const jbyte*>(hdr));
  if (len) env->SetByteArrayRegion(arr, 8, static_cast<jsize>(len), reinterpret_cast<const jbyte*>(out.data));
  return arr;
}

JNIEXPORT jint JNICALL Java_org_embermesh_app_core_EmberNative_guidesLoad(JNIEnv* env, jclass, jlong handle,
                                                                         jbyteArray pack) {
  EmberHandle* h = H(handle);
  if (!h || !pack) return -100;
  const jsize n = env->GetArrayLength(pack);
  jbyte* d = env->GetByteArrayElements(pack, nullptr);
  if (!d) return -101;
  int r = ember_guides_load(h->node, reinterpret_cast<const uint8_t*>(d), static_cast<size_t>(n), 1);
  env->ReleaseByteArrayElements(pack, d, JNI_ABORT);
  if (r == 0) h->guides.release();  // the core now owns a copy; an older zero-copy pack is no longer used
  return r;
}

// Zero-copy load of an APK asset. Store the asset uncompressed (aaptOptions noCompress "pack") so the
// buffer is a direct mmap of the APK; otherwise the asset manager inflates it into memory once.
JNIEXPORT jint JNICALL Java_org_embermesh_app_core_EmberNative_guidesLoadAsset(JNIEnv* env, jclass, jlong handle,
                                                                              jobject asset_manager, jstring path) {
  EmberHandle* h = H(handle);
  if (!h || !asset_manager || !path) return -100;
  AAssetManager* am = AAssetManager_fromJava(env, asset_manager);
  if (!am) return -101;
  const char* p = env->GetStringUTFChars(path, nullptr);
  if (!p) return -102;
  AAsset* a = AAssetManager_open(am, p, AASSET_MODE_BUFFER);
  env->ReleaseStringUTFChars(path, p);
  if (!a) return -103;
  const void* buf = AAsset_getBuffer(a);
  const off_t len = AAsset_getLength(a);
  if (!buf || len <= 0) {
    AAsset_close(a);
    return -104;
  }
  int r = ember_guides_load(h->node, static_cast<const uint8_t*>(buf), static_cast<size_t>(len), 0);
  if (r != 0) {
    AAsset_close(a);
    return r;
  }
  h->guides.release();
  h->guides.asset = a;
  return 0;
}

// Zero-copy load of a pack file on disk (e.g. an updated guides.pack received over the mesh).
JNIEXPORT jint JNICALL Java_org_embermesh_app_core_EmberNative_guidesLoadFile(JNIEnv* env, jclass, jlong handle,
                                                                             jstring path) {
  EmberHandle* h = H(handle);
  if (!h || !path) return -100;
  const char* p = env->GetStringUTFChars(path, nullptr);
  if (!p) return -102;
  int fd = open(p, O_RDONLY | O_CLOEXEC);
  env->ReleaseStringUTFChars(path, p);
  if (fd < 0) return -103;
  struct stat st {};
  if (fstat(fd, &st) != 0 || st.st_size <= 0) {
    close(fd);
    return -104;
  }
  void* m = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (m == MAP_FAILED) return -105;
  int r = ember_guides_load(h->node, static_cast<const uint8_t*>(m), static_cast<size_t>(st.st_size), 0);
  if (r != 0) {
    munmap(m, static_cast<size_t>(st.st_size));
    return r;
  }
  h->guides.release();
  h->guides.map = m;
  h->guides.map_len = static_cast<size_t>(st.st_size);
  return 0;
}

}  // extern "C"
