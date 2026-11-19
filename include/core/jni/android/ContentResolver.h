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

#ifndef CORAL_JNI_ANDROID_CONTENT_RESOLVER_H
#define CORAL_JNI_ANDROID_CONTENT_RESOLVER_H

#include <core/jni/JniScopedRef.h>
#include <core/jni/android/AssetFileDescriptor.h>

#include <memory>
#include <string>

namespace jni {

/**
 * The jni wrapper of the android.content.ContentResolver.
 *
 * <p>Only the calls that hand out a descriptor are wrapped: a data source reads at arbitrary
 * positions, which the stream of a provider does not offer.
 */
class ContentResolver final {
public:
  /**
   * Wraps a {@code ContentResolver}. Returns null when there is none.
   */
  static std::unique_ptr<ContentResolver> create(jobject contentResolver);

  ~ContentResolver();

  ContentResolver(const ContentResolver& other) = delete;

  ContentResolver& operator=(const ContentResolver& other) = delete;

  /**
   * Opens the descriptor of a content uri.
   *
   * @param uri The content uri.
   * @param mode The mode the provider is opened with, {@code "r"} by default.
   *
   * @return the descriptor, or null when the provider refuses the uri.
   */
  std::unique_ptr<AssetFileDescriptor> openAssetFileDescriptor(const std::string& uri,
    const std::string& mode = "r") const;

  /**
   * Opens the descriptor of a content uri with the type the caller expects.
   *
   * <p>The media the platform stores is asked for in its original format: a media provider would
   * otherwise be allowed to transcode it, which is neither the media the application asked for nor
   * seekable media in general.
   */
  std::unique_ptr<AssetFileDescriptor> openTypedAssetFileDescriptor(const std::string& uri,
    const std::string& mimeType) const;

  [[nodiscard]] jobject obj() const { return resolver_.obj(); }

  [[nodiscard]] bool isNull() const { return resolver_.is_null(); }

private:
  ContentResolver(JNIEnv* env, const JavaRef<jobject>& contentResolver);

  ScopedJavaGlobalRef<jobject> resolver_;
  jmethodID openAssetFileDescriptor_ = nullptr;
  jmethodID openTypedAssetFileDescriptor_ = nullptr;
};

} // namespace jni

using JniContentResolver = jni::ContentResolver;

#endif //CORAL_JNI_ANDROID_CONTENT_RESOLVER_H
