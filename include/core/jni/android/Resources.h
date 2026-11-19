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

#ifndef CORAL_JNI_ANDROID_RESOURCES_H
#define CORAL_JNI_ANDROID_RESOURCES_H

#include <core/jni/JniScopedRef.h>
#include <core/jni/android/AssetFileDescriptor.h>

#include <memory>
#include <string>

namespace jni {

/**
 * The jni wrapper of the android.content.res.Resources.
 */
class Resources final {
public:
  /**
   * Wraps a {@code Resources}. Returns null when there is none.
   */
  static std::unique_ptr<Resources> create(jobject resources);

  ~Resources();

  Resources(const Resources& other) = delete;

  Resources& operator=(const Resources& other) = delete;

  /**
   * Opens the descriptor of a raw resource.
   *
   * <p>The platform refuses a resource that is stored compressed, for which there is no region of a
   * file to describe.
   *
   * @return the descriptor, or null when the resource is compressed or does not exist.
   */
  std::unique_ptr<AssetFileDescriptor> openRawResourceFd(int resourceId) const;

  /**
   * Returns the identifier of a resource, or 0 when the package has no such resource.
   */
  [[nodiscard]] int getIdentifier(const std::string& name, const std::string& defType,
    const std::string& defPackage) const;

  [[nodiscard]] jobject obj() const { return resources_.obj(); }

  [[nodiscard]] bool isNull() const { return resources_.is_null(); }

private:
  Resources(JNIEnv* env, const JavaRef<jobject>& resources);

  ScopedJavaGlobalRef<jobject> resources_;
  jmethodID openRawResourceFd_ = nullptr;
  jmethodID getIdentifier_ = nullptr;
};

} // namespace jni

using JniResources = jni::Resources;

#endif //CORAL_JNI_ANDROID_RESOURCES_H
