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

#include <core/jni/android/AssetFileDescriptor.h>

#include <core/jni/JniHelper.h>
#include <core/jni/JvmEnv.h>

namespace jni {

namespace {
constexpr const char* kClassName = "android/content/res/AssetFileDescriptor";
} // namespace anonymous

AssetFileDescriptor::AssetFileDescriptor(JNIEnv* env, const JavaRef<jobject>& assetFileDescriptor)
  : descriptor_(env, assetFileDescriptor) {
  if (env == nullptr || descriptor_.is_null()) {
    return;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass(kClassName));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  getStartOffset_ = env->GetMethodID(clazz.obj(), "getStartOffset", "()J");
  getLength_ = env->GetMethodID(clazz.obj(), "getLength", "()J");
  getFileDescriptor_ = env->GetMethodID(clazz.obj(), "getFileDescriptor",
      "()Ljava/io/FileDescriptor;");
  close_ = env->GetMethodID(clazz.obj(), "close", "()V");
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  open_ = true;
}

AssetFileDescriptor::~AssetFileDescriptor() {
  close();
}

std::unique_ptr<AssetFileDescriptor> AssetFileDescriptor::create(const jobject assetFileDescriptor) {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || assetFileDescriptor == nullptr) {
    return nullptr;
  }
  auto held = ScopedJavaLocalRef<jobject>(env, env->NewLocalRef(assetFileDescriptor));
  return std::unique_ptr<AssetFileDescriptor>(new AssetFileDescriptor(env, held));
}

long AssetFileDescriptor::getStartOffset() const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || getStartOffset_ == nullptr || descriptor_.is_null()) {
    return 0;
  }
  const jlong offset = env->CallLongMethod(descriptor_.obj(), getStartOffset_);
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return 0;
  }
  return static_cast<long>(offset);
}

long AssetFileDescriptor::getLength() const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || getLength_ == nullptr || descriptor_.is_null()) {
    return kUnknownLength;
  }
  const jlong length = env->CallLongMethod(descriptor_.obj(), getLength_);
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return kUnknownLength;
  }
  return static_cast<long>(length);
}

std::unique_ptr<FileDescriptor> AssetFileDescriptor::getFileDescriptor() const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || getFileDescriptor_ == nullptr || descriptor_.is_null()) {
    return nullptr;
  }
  auto fileDescriptor = ScopedJavaLocalRef<jobject>(
    env, env->CallObjectMethod(descriptor_.obj(), getFileDescriptor_));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (fileDescriptor.is_null()) {
    return nullptr;
  }
  // The reference was created by this call alone, therefore the wrapper may take it over.
  return FileDescriptor::create(fileDescriptor.obj());
}

void AssetFileDescriptor::close() {
  if (!open_) {
    return;
  }
  open_ = false;
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || close_ == nullptr || descriptor_.is_null()) {
    return;
  }
  env->CallVoidMethod(descriptor_.obj(), close_);
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
  }
}

} // namespace jni
