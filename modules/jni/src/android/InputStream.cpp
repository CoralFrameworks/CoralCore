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

#include <core/jni/android/InputStream.h>

#include <core/jni/JniHelper.h>
#include <core/jni/JvmEnv.h>

#include <limits>

namespace jni {

namespace {
constexpr const char* kClassName = "java/io/InputStream";
} // namespace anonymous

InputStream::InputStream(JNIEnv* env, const JavaRef<jobject>& inputStream)
  : stream_(env, inputStream) {
  if (env == nullptr || stream_.is_null()) {
    return;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass(kClassName));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  read_ = env->GetMethodID(clazz.obj(), "read", "([BII)I");
  skip_ = env->GetMethodID(clazz.obj(), "skip", "(J)J");
  available_ = env->GetMethodID(clazz.obj(), "available", "()I");
  close_ = env->GetMethodID(clazz.obj(), "close", "()V");
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  open_ = true;
}

InputStream::~InputStream() {
  close();
}

std::unique_ptr<InputStream> InputStream::create(const jobject inputStream) {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || inputStream == nullptr) {
    return nullptr;
  }
  auto held = ScopedJavaLocalRef<jobject>(env, env->NewLocalRef(inputStream));
  return std::unique_ptr<InputStream>(new InputStream(env, held));
}

int InputStream::read(uint8_t* buffer, size_t length) {
  if (!open_ || buffer == nullptr || read_ == nullptr || stream_.is_null()) {
    return -1;
  }
  if (length == 0) {
    return 0;
  }
  if (length > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
    length = static_cast<size_t>(std::numeric_limits<jsize>::max());
  }
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return -1;
  }
  auto array = ScopedJavaLocalRef<jbyteArray>(env, env->NewByteArray(static_cast<jsize>(length)));
  if (array.is_null()) {
    JniHelper::RecordJavaException(env);
    return -1;
  }
  const jint read = env->CallIntMethod(stream_.obj(), read_, array.obj(), 0,
    static_cast<jint>(length));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return -1;
  }
  if (read <= 0) {
    return -1;
  }
  env->GetByteArrayRegion(array.obj(), 0, read, reinterpret_cast<jbyte*>(buffer));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return -1;
  }
  return static_cast<int>(read);
}

long InputStream::skip(const long count) {
  if (!open_ || skip_ == nullptr || stream_.is_null()) {
    return 0;
  }
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return 0;
  }
  const jlong skipped = env->CallLongMethod(stream_.obj(), skip_, static_cast<jlong>(count));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return 0;
  }
  return static_cast<long>(skipped);
}

int InputStream::available() const {
  if (!open_ || available_ == nullptr || stream_.is_null()) {
    return -1;
  }
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return -1;
  }
  const jint available = env->CallIntMethod(stream_.obj(), available_);
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return -1;
  }
  return static_cast<int>(available);
}

void InputStream::close() {
  if (!open_) {
    return;
  }
  open_ = false;
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || close_ == nullptr || stream_.is_null()) {
    return;
  }
  env->CallVoidMethod(stream_.obj(), close_);
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
  }
}

} // namespace jni
