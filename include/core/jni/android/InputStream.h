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

#ifndef CORAL_JNI_ANDROID_INPUT_STREAM_H
#define CORAL_JNI_ANDROID_INPUT_STREAM_H

#include <core/jni/JniScopedRef.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace jni {

/**
 * The jni wrapper of the java.io.InputStream.
 *
 * <p>The wrapper reads into a buffer of the caller, which costs one byte array of the platform per
 * read: it is used for the assets the platform does not hand out a descriptor for, and not on the
 * path of a data source that reads a region of a file.
 */
class InputStream final {
public:
  /**
   * Wraps an {@code InputStream}. Returns null when there is none.
   */
  static std::unique_ptr<InputStream> create(jobject inputStream);

  ~InputStream();

  InputStream(const InputStream& other) = delete;

  InputStream& operator=(const InputStream& other) = delete;

  /**
   * Reads up to {@code length} bytes.
   *
   * @return the number of bytes read, or -1 at the end of the stream or on error.
   */
  int read(uint8_t* buffer, size_t length);

  /**
   * Skips {@code count} bytes.
   *
   * @return the number of bytes that were skipped, which may be fewer than requested.
   */
  long skip(long count);

  /**
   * Returns the number of bytes that can be read without blocking, or -1 when the platform refuses.
   */
  int available() const;

  /**
   * Closes the stream. Idempotent.
   */
  void close();

  [[nodiscard]] jobject obj() const { return stream_.obj(); }

  [[nodiscard]] bool isNull() const { return stream_.is_null(); }

  [[nodiscard]] bool isOpen() const { return open_; }

private:
  InputStream(JNIEnv* env, const JavaRef<jobject>& inputStream);

  ScopedJavaGlobalRef<jobject> stream_;
  jmethodID read_ = nullptr;
  jmethodID skip_ = nullptr;
  jmethodID available_ = nullptr;
  jmethodID close_ = nullptr;
  bool open_ = false;
};

} // namespace jni

using JniInputStream = jni::InputStream;

#endif //CORAL_JNI_ANDROID_INPUT_STREAM_H
