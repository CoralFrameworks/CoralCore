/*
 * Copyright (C) 2023 The Graphifx Project
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

#ifndef CORAL_JNI_ANDROID_BYTE_BUFFER_H
#define CORAL_JNI_ANDROID_BYTE_BUFFER_H

#include <core/jni/JniScopedRef.h>

#include <memory>
#include <vector>

namespace jni {
/**
 * The jni wrapper of the java.nio.ByteBuffer
 */
class ByteBuffer {
public:
  static std::unique_ptr<ByteBuffer> allocate(int capacity);

  static std::unique_ptr<ByteBuffer> wrap(uint8_t* array, int offset, int length);

  static std::unique_ptr<ByteBuffer> wrap(const std::vector<uint8_t>& array);

  static std::unique_ptr<ByteBuffer> createWithObject(JNIEnv* env, jobject byteBufferObj);

  /**
   * Gets array from the JniByteBuffer.
   *
   * @param array the array to hold data
   * @param arrayByteSize the max size of the array.
   *
   * @return the real size to read.
   */
  size_t getByteArray(uint8_t *array, size_t arrayByteSize);

  /**
   * Writes array to the JniByteBuffer.
   *
   * @param array         the array to write
   * @param arrayByteSize the size of the array.
   */
  void writeByteArray(const uint8_t* array, size_t arrayByteSize);

  jobject obj() const { return byteBufferObj.obj(); }

private:
  void prepare(const JavaLocalRef<jclass> &clazzObj, const JavaLocalRef<jobject> &objRef);

  JavaGlobalRef<jclass> clazz;
  JavaGlobalRef<jobject> byteBufferObj;
};

} // namespace jni

using JniByteBuffer = jni::ByteBuffer;

#endif // CORAL_JNI_ANDROID_BYTE_BUFFER_H
