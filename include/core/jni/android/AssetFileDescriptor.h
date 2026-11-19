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

#ifndef CORAL_JNI_ANDROID_ASSET_FILE_DESCRIPTOR_H
#define CORAL_JNI_ANDROID_ASSET_FILE_DESCRIPTOR_H

#include <core/jni/JniScopedRef.h>
#include <core/jni/android/FileDescriptor.h>

#include <memory>

namespace jni {

/**
 * The jni wrapper of the android.content.res.AssetFileDescriptor.
 *
 * <p>An asset, a raw resource and the media of a content uri are all a region of a file: the
 * platform hands out the descriptor of that file and the offset and length of the region, which is
 * what lets a data source read them without copying them to a path of its own.
 *
 * <p>The descriptor stays open for as long as this wrapper does, or until {@link close} is called:
 * {@link FileDescriptor} only borrows it, and reading a descriptor whose owner closed it is a use
 * after free of the platform.
 */
class AssetFileDescriptor final {
public:
  /**
   * The length of an asset that extends to the end of its file, the counterpart of
   * {@code AssetFileDescriptor.UNKNOWN_LENGTH}.
   */
  static constexpr long kUnknownLength = -1;

  /**
   * Wraps an {@code AssetFileDescriptor}. Returns null when there is none.
   */
  static std::unique_ptr<AssetFileDescriptor> create(jobject assetFileDescriptor);

  ~AssetFileDescriptor();

  AssetFileDescriptor(const AssetFileDescriptor& other) = delete;

  AssetFileDescriptor& operator=(const AssetFileDescriptor& other) = delete;

  /**
   * Returns the offset of the asset in the file of its descriptor.
   */
  [[nodiscard]] long getStartOffset() const;

  /**
   * Returns the length of the asset, or {@link kUnknownLength} when it extends to the end of the file.
   */
  [[nodiscard]] long getLength() const;

  /**
   * Returns the descriptor of the file the asset is a region of, or null when it has none.
   */
  [[nodiscard]] std::unique_ptr<FileDescriptor> getFileDescriptor() const;

  /**
   * Closes the descriptor. The platform's {@code FileDescriptor} of it becomes invalid.
   */
  void close();

  [[nodiscard]] jobject obj() const { return descriptor_.obj(); }

  [[nodiscard]] bool isNull() const { return descriptor_.is_null(); }

  [[nodiscard]] bool isOpen() const { return open_; }

private:
  AssetFileDescriptor(JNIEnv* env, const JavaRef<jobject>& assetFileDescriptor);

  ScopedJavaGlobalRef<jobject> descriptor_;
  jmethodID getStartOffset_ = nullptr;
  jmethodID getLength_ = nullptr;
  jmethodID getFileDescriptor_ = nullptr;
  jmethodID close_ = nullptr;
  bool open_ = false;
};

} // namespace jni

using JniAssetFileDescriptor = jni::AssetFileDescriptor;

#endif //CORAL_JNI_ANDROID_ASSET_FILE_DESCRIPTOR_H
