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

#ifndef CORAL_JNI_SCOPED_BYTE_ARRAY_H
#define CORAL_JNI_SCOPED_BYTE_ARRAY_H

#include <core/jni/JniScopedRef.h>

namespace jni {

class JavaByteArray {
public:
  explicit JavaByteArray(JNIEnv* env, const ScopedLocalByteArray& arr, jint mode = 0);
  ~JavaByteArray();

  JavaByteArray(const JavaByteArray&) = delete;
  JavaByteArray& operator=(const JavaByteArray&) = delete;

  [[nodiscard]] uint8_t* data() const noexcept { return reinterpret_cast<uint8_t*>(mData); }
  [[nodiscard]] size_t   size() const noexcept { return mSize; }
  [[nodiscard]] bool     valid() const noexcept { return mData != nullptr; }

  void commit();

private:
  JNIEnv*    mEnv;
  ScopedLocalByteArray mArray;
  jbyte*     mData = nullptr;
  size_t     mSize = 0;
  jint       mMode = 0;
  bool       mReleased = false;
};

} // namespace jni

using ScopedByteArray = jni::JavaByteArray;

#endif //CORAL_JNI_SCOPED_BYTE_ARRAY_H
