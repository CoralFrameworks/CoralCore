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

#ifndef CORAL_JNI_ANDROID_FILE_DESCRIPTOR_H
#define CORAL_JNI_ANDROID_FILE_DESCRIPTOR_H

#include <core/jni/JniScopedRef.h>

#include <memory>

namespace jni {

/**
 * The jni wrapper of the java.io.FileDescriptor.
 *
 * <p>Only the number of the descriptor is read, not a stream that wraps it: a data source reads at
 * arbitrary positions, which a {@code FileInputStream} does not offer, while the descriptor itself
 * can be read with {@code pread}.
 *
 * <p>The descriptor is not closed by this wrapper: it belongs to the object the platform handed out,
 * e.g. an {@code AssetFileDescriptor}, which closes it.
 */
class FileDescriptor final {
public:
  /**
   * Wraps a {@code java.io.FileDescriptor}. Returns null when there is none.
   */
  static std::unique_ptr<FileDescriptor> create(jobject fileDescriptor);

  ~FileDescriptor();

  FileDescriptor(const FileDescriptor& other) = delete;

  FileDescriptor& operator=(const FileDescriptor& other) = delete;

  /**
   * Returns the number of the descriptor, or -1 when it could not be read.
   */
  [[nodiscard]] int getInt() const;

  /**
   * Returns whether the descriptor is still valid, i.e. was not closed by its owner.
   */
  [[nodiscard]] bool isValid() const;

  [[nodiscard]] jobject obj() const { return descriptor_.obj(); }

  [[nodiscard]] bool isNull() const { return descriptor_.is_null(); }

private:
  /// The state of {@link fd_} before the number of the descriptor was read.
  static constexpr int kUnresolved = -2;

  FileDescriptor(JNIEnv* env, const JavaRef<jobject>& fileDescriptor);

  ScopedJavaGlobalRef<jobject> descriptor_;
  jfieldID descriptorField_ = nullptr;
  jmethodID valid_ = nullptr;
  mutable int fd_ = kUnresolved;
};

} // namespace jni

using JniFileDescriptor = jni::FileDescriptor;

#endif //CORAL_JNI_ANDROID_FILE_DESCRIPTOR_H
