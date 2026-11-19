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

#ifndef CORAL_JNI_ANDROID_CONTEXT_H
#define CORAL_JNI_ANDROID_CONTEXT_H

#include <core/jni/JniScopedRef.h>
#include <core/jni/android/AssetManager.h>
#include <core/jni/android/ContentResolver.h>
#include <core/jni/android/Resources.h>

#include <memory>
#include <string>

namespace jni {

/**
 * The jni wrapper of the android.content.Context.
 */
class Context final {
public:
  /**
   * Wraps a {@code Context}. Returns null when there is none.
   */
  static std::unique_ptr<Context> create(jobject context);

  ~Context();

  Context(const Context& other) = delete;

  Context& operator=(const Context& other) = delete;

  /**
   * Returns the application context of this context.
   */
  [[nodiscard]] std::unique_ptr<Context> getApplicationContext() const;

  [[nodiscard]] std::unique_ptr<AssetManager> getAssets() const;

  [[nodiscard]] std::unique_ptr<ContentResolver> getContentResolver() const;

  [[nodiscard]] std::unique_ptr<Resources> getResources() const;

  /**
   * Returns the resources of an application, which is what a resource of the APK of another package
   * is read with.
   */
  [[nodiscard]] std::unique_ptr<Resources> getResourcesForApplication(
    const std::string& packageName) const;

  /**
   * Returns the name of the package of this context.
   */
  [[nodiscard]] std::string getPackageName() const;

  [[nodiscard]] jobject obj() const { return context_.obj(); }

  [[nodiscard]] bool isNull() const { return context_.is_null(); }

private:
  Context(JNIEnv* env, const JavaRef<jobject>& context);

  ScopedJavaGlobalRef<jobject> context_;
  jmethodID getApplicationContext_ = nullptr;
  jmethodID getAssets_ = nullptr;
  jmethodID getContentResolver_ = nullptr;
  jmethodID getResources_ = nullptr;
  jmethodID getPackageManager_ = nullptr;
  jmethodID getPackageName_ = nullptr;
};

/**
 * The application context of the process, as the platform handed it to the library.
 *
 * <p>The data sources of {@code libs/datasource} read an asset, a raw resource or the media of a
 * content uri through the platform, which needs a {@code Context}, while the data source itself is
 * named by a uri: the context is registered once, when the library loads, and read when a uri that
 * needs it is opened.
 *
 * <p>The context is a global reference, and the application context is used rather than the one the
 * registration was called with, so that a registration from an activity cannot keep the activity
 * alive.
 */
class ApplicationContext final {
public:
  /**
   * Registers the context of the process. Called once, e.g. from the application.
   *
   * @param context The context, e.g. the context of the application.
   */
  static void initialize(jobject context);

  /**
   * Returns whether a context was registered.
   */
  [[nodiscard]] static bool isInitialized();

  /**
   * Returns the registered context, or null when none was registered.
   */
  [[nodiscard]] static std::unique_ptr<Context> get();

  /**
   * Releases the registered context. Called when the library unloads.
   */
  static void release();

private:
  ApplicationContext() = delete;
};

} // namespace jni

using JniContext = jni::Context;
using JniApplicationContext = jni::ApplicationContext;

#endif //CORAL_JNI_ANDROID_CONTEXT_H
