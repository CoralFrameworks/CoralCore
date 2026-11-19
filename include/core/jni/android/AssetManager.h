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

#ifndef CORAL_JNI_ANDROID_ASSET_MANAGER_H
#define CORAL_JNI_ANDROID_ASSET_MANAGER_H

#include <core/jni/JniScopedRef.h>
#include <core/jni/android/AssetFileDescriptor.h>
#include <core/jni/android/InputStream.h>

#include <memory>
#include <string>
#include <vector>

namespace jni {

/**
 * The jni wrapper of the android.content.res.AssetManager.
 */
class AssetManager final {
public:
  /**
   * How an asset is opened, the counterpart of {@code AssetManager.ACCESS_*}.
   */
  enum AccessMode : int {
    kAccessUnknown = 0,
    kAccessRandom = 1,
    kAccessStreaming = 2,
    kAccessBuffer = 3,
  };

  /**
   * Wraps an {@code AssetManager}. Returns null when there is none.
   */
  static std::unique_ptr<AssetManager> create(jobject assetManager);

  ~AssetManager();

  AssetManager(const AssetManager& other) = delete;

  AssetManager& operator=(const AssetManager& other) = delete;

  /**
   * Opens an asset as a stream.
   *
   * @param path The path of the asset, relative to the assets directory, e.g. {@code video/a.mp4}.
   * @param mode How the platform should open the asset; {@link kAccessRandom} serves a data source
   * that reads at arbitrary positions.
   *
   * @return the stream, or null when the asset does not exist.
   */
  std::unique_ptr<InputStream> open(const std::string& path,
    AccessMode mode = kAccessRandom) const;

  /**
   * Opens the descriptor of an asset.
   *
   * <p>Only an asset that is stored in the APK uncompressed has a descriptor; the platform refuses
   * the others, which {@link open} serves instead.
   *
   * @return the descriptor, or null when the asset is compressed or does not exist.
   */
  std::unique_ptr<AssetFileDescriptor> openFd(const std::string& path) const;

  /**
   * Lists the files and directories of an asset directory.
   */
  [[nodiscard]] std::vector<std::string> list(const std::string& path) const;

  [[nodiscard]] jobject obj() const { return manager_.obj(); }

  [[nodiscard]] bool isNull() const { return manager_.is_null(); }

private:
  AssetManager(JNIEnv* env, const JavaRef<jobject>& assetManager);

  ScopedJavaGlobalRef<jobject> manager_;
  jmethodID open_ = nullptr;
  jmethodID openFd_ = nullptr;
  jmethodID list_ = nullptr;
};

} // namespace jni

using JniAssetManager = jni::AssetManager;

#endif //CORAL_JNI_ANDROID_ASSET_MANAGER_H
