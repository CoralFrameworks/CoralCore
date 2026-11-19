/*
 * Copyright (C) 2022 The Coral Project
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

#ifndef CORAL_JNI_HELPER_H
#define CORAL_JNI_HELPER_H

#include <jni.h>
#include <string>

namespace jni {

/**
 * Helper functions for JNI calls
 */
class Helper final {
private:
  Helper() = default;
  virtual ~Helper() = default;

public:
  Helper(Helper&&) = delete;
  Helper(const Helper&) = delete;
  Helper& operator=(Helper&&) = delete;
  Helper& operator=(const Helper& rhs) = delete;

  /**
   * Returns an instance of the Helper.
   */
  static Helper* GetInstance();

  /**
   * Detach current thread from jni
   */
  static void DetachCurrentThread();

  /**
   * Convert string with character code UTF-8
   * @param str pointer to a string which is encoded UTF-8
   * @return converted input string as an UTF-8 std::string
   */
  static jstring ConvertString(JNIEnv* env, const std::string& str);

  /**
   * Convert string with character code UTF-8
   * @param str pointer to a string which is encoded UTF-8
   * @return converted input string as an UTF-8 std::string
   */
  static std::string ConvertUTFString(JNIEnv* env, jstring str);

  /**
   * Describes the Java exception that is pending on the environment, and clears it.
   *
   * A callback that throws has its reason swallowed by the JNI contract, which used to
   * leave a bare "the block raised an exception" in the log: this turns the throwable
   * itself (its class and message) into text so the caller can log why it failed.
   *
   * @return the description, or an empty string when no exception was pending
   */
  static std::string DescribeException(JNIEnv* env);

  /**
   * Describes the exception that is pending on the environment, clears it, and records the
   * description on this thread.
   *
   * A wrapper of a platform class reports a refusal of the platform (a file that does not exist, an
   * asset that is stored compressed) by returning null rather than by throwing, because the caller
   * has no exception of its own to catch: the description is kept for {@link takeLastError} instead
   * of being printed here, where it would be lost in the log of a render thread.
   */
  static void RecordJavaException(JNIEnv* env) noexcept;

  /**
   * Returns the description recorded by the last {@link RecordJavaException} of this thread, and
   * clears it.
   */
  static std::string takeLastError();

  /**
   * Throw an exception with the specified class and an optional message.
   *
   * The "className" argument will be passed directly to FindClass, which
   * takes strings with slashes (e.g. "java/lang/Object").
   *
   * If an exception is currently pending, we log a warning message and
   * clear it.
   *
   * Returns 0 on success, nonzero if something failed (e.g. the exception
   * class couldn't be found, so *an* exception will still be pending).
   *
   * Currently aborts the VM if it can't throw the exception.
   */
  static int ThrowException(JNIEnv* env, const char* className, const char* msg);

  /**
   * Throw an exception with the specified class and an optional message.
   *
   * The "className" argument will be passed directly to FindClass, which
   * takes strings with slashes (e.g. "java/lang/Object").
   *
   * If an exception is currently pending, we log a warning message and
   * clear it.
   *
   * Returns 0 on success, nonzero if something failed (e.g. the exception
   * class couldn't be found, so *an* exception will still be pending).
   *
   * Currently aborts the VM if it can't throw the exception.
   */
  int ThrowException(const char* className, const char* msg);
};

} // namespace jni

using JniHelper = jni::Helper;

#endif // CORAL_JNI_HELPER_H
