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

#include <core/jni/android/FileDescriptor.h>

#include <core/jni/JniHelper.h>
#include <core/jni/JvmEnv.h>

namespace jni {

namespace {

/**
 * Returns the field that holds the number of the descriptor.
 *
 * <p>Android names it {@code descriptor}; the desktop implementation of the platform names it
 * {@code fd}. Both are private, which a JNI field lookup does not mind.
 */
jfieldID findDescriptorField(JNIEnv* env, const jclass clazz) {
  jfieldID field = env->GetFieldID(clazz, "descriptor", "I");
  if (field != nullptr) {
    return field;
  }
  // The lookup of a name that does not exist leaves a NoSuchFieldError pending, which the second
  // lookup would refuse to run under.
  env->ExceptionClear();
  field = env->GetFieldID(clazz, "fd", "I");
  if (field != nullptr) {
    return field;
  }
  env->ExceptionClear();
  return nullptr;
}

} // namespace anonymous

FileDescriptor::FileDescriptor(JNIEnv* env, const JavaRef<jobject>& fileDescriptor)
  : descriptor_(env, fileDescriptor) {
  if (env == nullptr || descriptor_.is_null()) {
    return;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass("java/io/FileDescriptor"));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  descriptorField_ = findDescriptorField(env, clazz.obj());
  valid_ = env->GetMethodID(clazz.obj(), "valid", "()Z");
  if (valid_ == nullptr) {
    JniHelper::RecordJavaException(env);
  }
}

FileDescriptor::~FileDescriptor() = default;

std::unique_ptr<FileDescriptor> FileDescriptor::create(const jobject fileDescriptor) {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || fileDescriptor == nullptr) {
    return nullptr;
  }
  // The reference the caller owns is not taken over: a reference of our own holds the object for as
  // long as the wrapper does.
  auto held = ScopedJavaLocalRef<jobject>(env, env->NewLocalRef(fileDescriptor));
  return std::unique_ptr<FileDescriptor>(new FileDescriptor(env, held));
}

int FileDescriptor::getInt() const {
  if (fd_ != kUnresolved) {
    return fd_;
  }
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || descriptorField_ == nullptr || descriptor_.is_null()) {
    return -1;
  }
  const jint fd = env->GetIntField(descriptor_.obj(), descriptorField_);
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return -1;
  }
  fd_ = static_cast<int>(fd);
  return fd_;
}

bool FileDescriptor::isValid() const {
  if (valid_ == nullptr || descriptor_.is_null()) {
    return getInt() >= 0;
  }
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return false;
  }
  const jboolean valid = env->CallBooleanMethod(descriptor_.obj(), valid_);
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return false;
  }
  return valid == JNI_TRUE;
}

} // namespace jni
