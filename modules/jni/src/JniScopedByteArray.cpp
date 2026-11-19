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

#include <core/jni/JniScopedByteArray.h>

namespace jni {

JavaByteArray::JavaByteArray(JNIEnv *env, const ScopedLocalByteArray &arr, jint mode)
    : mEnv(env),
      mArray(arr),
      mData(nullptr),
      mSize(0),
      mMode(mode) {
  if (mArray.is_null()) {
    return;
  }

  mData = env->GetByteArrayElements(mArray.obj(), nullptr);
  if (!mData) {
    return;
  }
  mSize = static_cast<size_t>(env->GetArrayLength(mArray.obj()));
}

JavaByteArray::~JavaByteArray() {
  if (!mReleased && !mArray.is_null() && mData) {
    mEnv->ReleaseByteArrayElements(mArray.obj(), mData, mMode);
    mReleased = true;
  }
}

void JavaByteArray::commit() {
  if (!mReleased && !mArray.is_null() && mData) {
    mEnv->ReleaseByteArrayElements(mArray.obj(), mData, 0);
    mReleased = true;
  }
}

} // namespace jni
