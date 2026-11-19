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

#ifndef CORAL_JNI_ANDROID_URI_H
#define CORAL_JNI_ANDROID_URI_H

#include <core/jni/JniScopedRef.h>

#include <memory>
#include <string>
#include <vector>

namespace jni {

/**
 * The jni wrapper of the android.net.Uri.
 *
 * <p>The parser of the platform is used rather than one of the module: the scheme, the authority and
 * the path segments of a {@code content} uri are what the platform's providers were registered under,
 * and reading them with a parser of our own would be a second interpretation of the same text.
 */
class Uri final {
public:
  /**
   * Parses a uri. Returns null when the platform refuses it.
   */
  static std::unique_ptr<Uri> parse(const std::string& uri);

  ~Uri();

  Uri(const Uri& other) = delete;

  Uri& operator=(const Uri& other) = delete;

  /**
   * Returns the scheme of the uri, or an empty string when it has none.
   */
  [[nodiscard]] std::string getScheme() const;

  /**
   * Returns the decoded path of the uri, or an empty string when it has none.
   */
  [[nodiscard]] std::string getPath() const;

  /**
   * Returns the authority of the uri, e.g. the package name of an {@code android.resource} uri, or an
   * empty string when it has none.
   */
  [[nodiscard]] std::string getAuthority() const;

  /**
   * Returns the host of the uri, or an empty string when it has none.
   */
  [[nodiscard]] std::string getHost() const;

  /**
   * Returns the last segment of the path of the uri, or an empty string when it has none.
   */
  [[nodiscard]] std::string getLastPathSegment() const;

  /**
   * Returns the decoded segments of the path of the uri.
   */
  [[nodiscard]] std::vector<std::string> getPathSegments() const;

  /**
   * Returns the value of a query parameter of the uri, or an empty string when it is not set.
   */
  [[nodiscard]] std::string getQueryParameter(const std::string& key) const;

  /**
   * Returns the uri as a string.
   */
  [[nodiscard]] std::string toString() const;

  [[nodiscard]] jobject obj() const { return uri_.obj(); }

  [[nodiscard]] bool isNull() const { return uri_.is_null(); }

private:
  Uri(JNIEnv* env, const JavaRef<jobject>& uri);

  [[nodiscard]] std::string callString(jmethodID method) const;

  ScopedJavaGlobalRef<jobject> uri_;
  ScopedJavaGlobalRef<jclass> uriClass_;
  jmethodID getScheme_ = nullptr;
  jmethodID getPath_ = nullptr;
  jmethodID getAuthority_ = nullptr;
  jmethodID getHost_ = nullptr;
  jmethodID getLastPathSegment_ = nullptr;
  jmethodID getPathSegments_ = nullptr;
  jmethodID getQueryParameter_ = nullptr;
  jmethodID toString_ = nullptr;
};

} // namespace jni

using JniUri = jni::Uri;

#endif //CORAL_JNI_ANDROID_URI_H
