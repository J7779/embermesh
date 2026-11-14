// Phone hub: the host library's hub-only C API (host/include/ember_hub.h) behind JNI.
//
// The Kotlin NodeRunner drives the node itself, so the hub is wired as one more Kotlin endpoint
// (kind `ws_server`, core link type `raw`): frames from guests go to the Kotlin sink
// (-> ember_link_rx), frames the core writes to the hub link are broadcast to every guest. The hub
// runs its own loop thread inside the host library and never touches the node.
//
// Kotlin side: org.embermesh.app.hub.HubNative.
#include <jni.h>

#include <android/log.h>

#include <new>
#include <string>

#include "ember_hub.h"

namespace {

JavaVM* g_vm = nullptr;

// Attaches the hub's loop thread to the JVM once and detaches it when the thread exits.
struct JniThread {
  JNIEnv* env = nullptr;
  bool attached = false;
  ~JniThread() {
    if (attached && g_vm) g_vm->DetachCurrentThread();
  }
  JNIEnv* get() {
    if (env) return env;
    if (!g_vm) return nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return env;
    JavaVMAttachArgs a{JNI_VERSION_1_6, const_cast<char*>("ember-hub"), nullptr};
    if (g_vm->AttachCurrentThread(&env, &a) != JNI_OK) {
      env = nullptr;
      return nullptr;
    }
    attached = true;
    return env;
  }
};
thread_local JniThread t_jni;

struct Handle {
  ember_hub* hub = nullptr;
  jobject cb = nullptr;
  jmethodID on_frame = nullptr;
  jmethodID on_clients = nullptr;
};

void on_frame(void* user, const uint8_t* data, size_t len) {
  auto* h = static_cast<Handle*>(user);
  JNIEnv* env = t_jni.get();
  if (!env || !h->on_frame) return;
  jbyteArray a = env->NewByteArray(static_cast<jsize>(len));
  if (!a) {
    env->ExceptionClear();
    return;
  }
  env->SetByteArrayRegion(a, 0, static_cast<jsize>(len), reinterpret_cast<const jbyte*>(data));
  env->CallVoidMethod(h->cb, h->on_frame, a);
  if (env->ExceptionCheck()) env->ExceptionClear();
  env->DeleteLocalRef(a);
}

void on_clients(void* user, size_t guests) {
  auto* h = static_cast<Handle*>(user);
  JNIEnv* env = t_jni.get();
  if (!env || !h->on_clients) return;
  env->CallVoidMethod(h->cb, h->on_clients, static_cast<jint>(guests));
  if (env->ExceptionCheck()) env->ExceptionClear();
}

inline Handle* HB(jlong h) { return reinterpret_cast<Handle*>(static_cast<intptr_t>(h)); }

}  // namespace

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  g_vm = vm;
  return JNI_VERSION_1_6;
}

JNIEXPORT jlong JNICALL Java_org_embermesh_app_hub_HubNative_create(JNIEnv* env, jclass, jobject cb) {
  if (!cb) return 0;
  auto* h = new (std::nothrow) Handle();
  if (!h) return 0;
  h->cb = env->NewGlobalRef(cb);
  jclass c = env->GetObjectClass(cb);
  h->on_frame = env->GetMethodID(c, "onFrame", "([B)V");
  h->on_clients = env->GetMethodID(c, "onClients", "(I)V");
  env->DeleteLocalRef(c);
  h->hub = ember_hub_create(&on_frame, &on_clients, h);
  if (!h->hub) {
    env->DeleteGlobalRef(h->cb);
    delete h;
    return 0;
  }
  return static_cast<jlong>(reinterpret_cast<intptr_t>(h));
}

JNIEXPORT void JNICALL Java_org_embermesh_app_hub_HubNative_destroy(JNIEnv* env, jclass, jlong handle) {
  Handle* h = HB(handle);
  if (!h) return;
  ember_hub_destroy(h->hub);  // joins the loop thread: no callback runs after this
  env->DeleteGlobalRef(h->cb);
  delete h;
}

// options: ember_hub_start JSON. Returns null on success, else the error text.
JNIEXPORT jstring JNICALL Java_org_embermesh_app_hub_HubNative_start(JNIEnv* env, jclass, jlong handle, jstring options) {
  Handle* h = HB(handle);
  if (!h) return env->NewStringUTF("hub not created");
  const char* o = options ? env->GetStringUTFChars(options, nullptr) : nullptr;
  char err[256] = {0};
  int rc = ember_hub_start(h->hub, o ? o : "{}", err, sizeof err);
  if (o) env->ReleaseStringUTFChars(options, o);
  if (rc == 0) return nullptr;
  return env->NewStringUTF(err[0] ? err : "hub failed to start");
}

JNIEXPORT void JNICALL Java_org_embermesh_app_hub_HubNative_stop(JNIEnv*, jclass, jlong handle) {
  if (Handle* h = HB(handle)) ember_hub_stop(h->hub);
}

JNIEXPORT void JNICALL Java_org_embermesh_app_hub_HubNative_broadcast(JNIEnv* env, jclass, jlong handle, jbyteArray data) {
  Handle* h = HB(handle);
  if (!h || !data) return;
  const jsize n = env->GetArrayLength(data);
  jbyte* d = env->GetByteArrayElements(data, nullptr);
  if (!d) return;
  ember_hub_broadcast(h->hub, reinterpret_cast<const uint8_t*>(d), static_cast<size_t>(n));  // copies
  env->ReleaseByteArrayElements(data, d, JNI_ABORT);
}

JNIEXPORT jstring JNICALL Java_org_embermesh_app_hub_HubNative_status(JNIEnv* env, jclass, jlong handle) {
  Handle* h = HB(handle);
  char* s = h ? ember_hub_status(h->hub) : nullptr;
  jstring r = env->NewStringUTF(s ? s : "{\"running\":false}");
  ember_hub_free(s);
  return r;
}

JNIEXPORT void JNICALL Java_org_embermesh_app_hub_HubNative_setProbe(JNIEnv* env, jclass, jlong handle, jstring json) {
  Handle* h = HB(handle);
  if (!h || !json) return;
  const char* j = env->GetStringUTFChars(json, nullptr);
  if (!j) return;
  ember_hub_set_probe(h->hub, j);
  env->ReleaseStringUTFChars(json, j);
}

}  // extern "C"
