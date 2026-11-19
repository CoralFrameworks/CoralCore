/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(__ANDROID__)

#include <core/jni/android/SurfaceTexture.h>

#include <core/jni/JvmEnv.h>
#include <core/jni/android/Surface.h>

#include <android/log.h>

#include <atomic>

namespace jni {

namespace {

/** The JNI name of the class, used for `FindClass`. */
constexpr auto kClassName = "android/graphics/SurfaceTexture";

} // namespace anonymous

std::unique_ptr<SurfaceTexture> SurfaceTexture::create(JNIEnv* env, const int texName) {
  if (env == nullptr) {
    return nullptr;
  }
  JvmEnv::handleException(env);
  auto instance = std::unique_ptr<SurfaceTexture>(new SurfaceTexture());
  if (!instance->bind(env)) {
    return nullptr;
  }
  const auto constructor = env->GetMethodID(instance->surfaceTexture_class.obj(), "<init>", "(I)V");
  if (constructor == nullptr) {
    JvmEnv::handleException(env);
    return nullptr;
  }
  const auto object = ScopedJavaLocalRef<jobject>(
    env, env->NewObject(instance->surfaceTexture_class.obj(), constructor, texName));
  if (object.is_null()) {
    JvmEnv::handleException(env);
    return nullptr;
  }
  instance->surfaceTexture = object;
  return instance;
}

std::unique_ptr<SurfaceTexture> SurfaceTexture::createDetached(JNIEnv* env, const bool singleBufferMode) {
  if (env == nullptr) {
    return nullptr;
  }
  JvmEnv::handleException(env);
  auto instance = std::unique_ptr<SurfaceTexture>(new SurfaceTexture());
  if (!instance->bind(env)) {
    return nullptr;
  }
  // The constructor without a texture name leaves the object detached: the surface exists and the producer
  // can fill the queue, and `attachToGLContext` binds it to the renderer later. API 26 only.
  const auto constructor = env->GetMethodID(instance->surfaceTexture_class.obj(), "<init>", "(Z)V");
  if (constructor == nullptr) {
    __android_log_print(ANDROID_LOG_WARN, "coral-jni",
      "SurfaceTexture(boolean) does not exist on this API level");
    env->ExceptionClear();
    return nullptr;
  }
  const auto object = ScopedJavaLocalRef<jobject>(
    env, env->NewObject(instance->surfaceTexture_class.obj(), constructor,
    singleBufferMode ? JNI_TRUE : JNI_FALSE));
  if (object.is_null()) {
    JvmEnv::handleException(env);
    return nullptr;
  }
  instance->surfaceTexture = object;
  instance->mAttached = false;
  return instance;
}

SurfaceTexture::~SurfaceTexture() = default;

void* SurfaceTexture::get() const {
  return surfaceTexture.obj();
}

std::unique_ptr<Surface> SurfaceTexture::getSurface(JNIEnv* env) {
  if (env == nullptr || surfaceTexture.is_null()) {
    return nullptr;
  }
  JvmEnv::handleException(env);
  // `new Surface(surfaceTexture)` rather than `surfaceTexture.getSurface()`: the constructor is the older and
  // more widely present way to name the producer's surface, and the device this ran on answered
  // "no such method" for `getSurface` even though its framework documents it.
  const auto surfaceClass = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/view/Surface"));
  if (surfaceClass.is_null()) {
    JvmEnv::handleException(env);
    return nullptr;
  }
  const auto constructor = env->GetMethodID(surfaceClass.obj(), "<init>",
    "(Landroid/graphics/SurfaceTexture;)V");
  if (constructor == nullptr) {
    __android_log_print(ANDROID_LOG_WARN, "coral-jni",
      "android.view.Surface(SurfaceTexture) does not exist");
    env->ExceptionClear();
    return nullptr;
  }
  const auto object = ScopedJavaLocalRef<jobject>(
    env, env->NewObject(surfaceClass.obj(), constructor, surfaceTexture.obj()));
  if (object.is_null()) {
    JvmEnv::handleException(env);
    return nullptr;
  }
  return Surface::fromObject(env, object.obj());
}

void SurfaceTexture::setDefaultBufferSize(JNIEnv* env, const int width, const int height) {
  if (env == nullptr || surfaceTexture.is_null() || surfaceTexture_setDefaultBufferSize == nullptr) {
    return;
  }
  JvmEnv::handleException(env);
  env->CallVoidMethod(surfaceTexture.obj(), surfaceTexture_setDefaultBufferSize, width, height);
  JvmEnv::handleException(env);
}

bool SurfaceTexture::updateTexImage(JNIEnv* env) {
  if (env == nullptr || surfaceTexture.is_null() || surfaceTexture_updateTexImage == nullptr) {
    return false;
  }
  JvmEnv::handleException(env);
  env->CallVoidMethod(surfaceTexture.obj(), surfaceTexture_updateTexImage);
  // `updateTexImage` throws when no frame is queued, which is a normal answer rather than a failure: the
  // caller asks again next time. The first one of a run is described anyway, so that a path that never gets a
  // frame says why instead of only that it did not.
  if (env->ExceptionCheck()) {
    static std::atomic<bool> explained{ false };
    if (!explained.exchange(true)) {
      env->ExceptionDescribe();
    }
    env->ExceptionClear();
    return false;
  }
  return true;
}

void SurfaceTexture::getTransformMatrix(JNIEnv* env, float matrix[16]) const {
  if (env == nullptr || surfaceTexture.is_null() || matrix == nullptr || surfaceTexture_getTransformMatrix == nullptr) {
    return;
  }
  JvmEnv::handleException(env);
  const auto array = ScopedJavaLocalRef<jfloatArray>(env, env->NewFloatArray(16));
  if (array.is_null()) {
    JvmEnv::handleException(env);
    return;
  }
  env->CallVoidMethod(surfaceTexture.obj(), surfaceTexture_getTransformMatrix, array.obj());
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return;
  }
  env->GetFloatArrayRegion(array.obj(), 0, 16, matrix);
  JvmEnv::handleException(env);
}

int64_t SurfaceTexture::getTimestamp(JNIEnv* env) const {
  if (env == nullptr || surfaceTexture.is_null() || surfaceTexture_getTimestamp == nullptr) {
    return 0;
  }
  JvmEnv::handleException(env);
  const auto timestamp = env->CallLongMethod(surfaceTexture.obj(), surfaceTexture_getTimestamp);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return 0;
  }
  return timestamp;
}

void SurfaceTexture::release(JNIEnv* env) {
  if (env == nullptr || surfaceTexture.is_null()) {
    return;
  }
  JvmEnv::handleException(env);
  if (surfaceTexture_detachFromGLContext != nullptr) {
    env->CallVoidMethod(surfaceTexture.obj(), surfaceTexture_detachFromGLContext);
    JvmEnv::handleException(env);
  }
  if (surfaceTexture_release != nullptr) {
    env->CallVoidMethod(surfaceTexture.obj(), surfaceTexture_release);
    JvmEnv::handleException(env);
  }
  surfaceTexture = nullptr;
}

bool SurfaceTexture::attachToGLContext(JNIEnv* env, const int texName) {
  if (env == nullptr || surfaceTexture.is_null() || surfaceTexture_attachToGLContext == nullptr) {
    return false;
  }
  JvmEnv::handleException(env);
  env->CallVoidMethod(surfaceTexture.obj(), surfaceTexture_attachToGLContext, texName);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  mAttached = true;
  return true;
}

bool SurfaceTexture::detachFromGLContext(JNIEnv* env) {
  if (env == nullptr || surfaceTexture.is_null() || surfaceTexture_detachFromGLContext == nullptr) {
    return false;
  }
  mAttached = false;
  JvmEnv::handleException(env);
  env->CallVoidMethod(surfaceTexture.obj(), surfaceTexture_detachFromGLContext);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  return true;
}

bool SurfaceTexture::isAttached() const {
  return mAttached;
}

bool SurfaceTexture::isValid() const {
  return !surfaceTexture.is_null();
}

bool SurfaceTexture::bind(JNIEnv* env) {
  JvmEnv::handleException(env);
  surfaceTexture_class = ScopedJavaLocalRef<jclass>(env, env->FindClass(kClassName));
  if (surfaceTexture_class.is_null()) {
    __android_log_print(ANDROID_LOG_WARN, "coral-jni",
      "android.graphics.SurfaceTexture is not visible from this thread");
    JvmEnv::handleException(env);
    return false;
  }
  // A missing method leaves a pending `NoSuchMethodError`, and every `GetMethodID` after it answers null too,
  // so the name is reported here and the exception is cleared, which keeps the next lookup meaningful.
  const auto method = [env, this](const char* name, const char* signature) -> jmethodID {
    const jmethodID id = env->GetMethodID(surfaceTexture_class.obj(), name, signature);
    if (id == nullptr) {
      __android_log_print(ANDROID_LOG_WARN, "coral-jni", "SurfaceTexture.%s does not exist", name);
      env->ExceptionClear();
    }
    return id;
  };
  surfaceTexture_updateTexImage = method("updateTexImage", "()V");
  surfaceTexture_getTransformMatrix = method("getTransformMatrix", "([F)V");
  surfaceTexture_getTimestamp = method("getTimestamp", "()J");
  surfaceTexture_setDefaultBufferSize = method("setDefaultBufferSize", "(II)V");
  surfaceTexture_detachFromGLContext = method("detachFromGLContext", "()V");
  surfaceTexture_attachToGLContext = method("attachToGLContext", "(I)V");
  surfaceTexture_release = method("release", "()V");
  if (surfaceTexture_updateTexImage == nullptr) {
    return false;
  }
  JvmEnv::handleException(env);
  return true;
}

} // namespace jni

#endif // __ANDROID__










