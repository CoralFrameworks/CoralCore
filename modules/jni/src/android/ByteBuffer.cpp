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

#include <core/jni/android/ByteBuffer.h>

namespace jni {

std::unique_ptr<ByteBuffer> ByteBuffer::allocate(int capacity) {
  auto* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return nullptr;
  }
  JvmEnv::handleException(env);

  auto clazzObj = env->FindClass("java/nio/ByteBuffer");
  auto allocate = env->GetStaticMethodID(clazzObj, "allocateDirect", "(I)Ljava/nio/ByteBuffer;");
  auto obj = env->CallStaticObjectMethod(clazzObj, allocate, capacity);

  auto byteBufferRef = ScopedJavaLocalRef<jobject>(env, obj);
  auto clazzRef = ScopedJavaLocalRef<jclass>(env, clazzObj);
  auto byteBuffer = std::make_unique<ByteBuffer>();
  byteBuffer->prepare(clazzRef, byteBufferRef);

  return byteBuffer;
}

std::unique_ptr<ByteBuffer> ByteBuffer::wrap(uint8_t* array, int offset, int length) {
  auto* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return nullptr;
  }
  JvmEnv::handleException(env);

  auto clazzObj = env->FindClass("java/nio/ByteBuffer");
  auto wrap = env->GetStaticMethodID(clazzObj, "wrap", "([BII)Ljava/nio/ByteBuffer;");
  auto buffer = env->NewByteArray((jsize)length + offset);
  env->SetByteArrayRegion(buffer, offset, (jsize)length,
      reinterpret_cast<const jbyte*>(array + offset));
  auto byteBufferObj =
      env->CallStaticObjectMethod(clazzObj, wrap, buffer, offset, length);

  auto byteBufferRef = ScopedJavaLocalRef<jobject>(env, byteBufferObj);
  auto clazzRef = ScopedJavaLocalRef<jclass>(env, clazzObj);
  auto byteBuffer = std::make_unique<ByteBuffer>();
  byteBuffer->prepare(clazzRef, byteBufferRef);

  return byteBuffer;
}

std::unique_ptr<ByteBuffer> ByteBuffer::wrap(const std::vector<uint8_t>& array) {
  auto* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return nullptr;
  }
  JvmEnv::handleException(env);

  auto length = array.size();

  auto clazzObj = env->FindClass("java/nio/ByteBuffer");
  auto mid_wrap = env->GetStaticMethodID(clazzObj, "wrap", "([B)Ljava/nio/ByteBuffer;");
  auto byteArray = env->NewByteArray((jsize)length);
  env->SetByteArrayRegion(byteArray, 0, (jsize)length, (const jbyte*)array.data());
  auto byteBufferObj = env->CallStaticObjectMethod(clazzObj, mid_wrap, byteArray);

  auto byteBufferRef = JavaLocalRef<jobject>(env, byteBufferObj);
  auto clazzRef = JavaLocalRef<jclass>(env, clazzObj);
  auto byteBuffer = std::make_unique<ByteBuffer>();
  byteBuffer->prepare(clazzRef, byteBufferRef);

  return byteBuffer;
}

std::unique_ptr<ByteBuffer> ByteBuffer::createWithObject(JNIEnv* env, jobject byteBufferObj) {
  JvmEnv::handleException(env);
  auto clazzObj = JavaLocalRef<jclass>(env, env->FindClass("java/nio/ByteBuffer"));
  auto bufferObj = JavaLocalRef<jobject>(env, byteBufferObj);
  auto jniBuffer = std::make_unique<ByteBuffer>();
  jniBuffer->prepare(clazzObj, bufferObj);
  return jniBuffer;
}

size_t ByteBuffer::getByteArray(uint8_t* array, size_t arrayByteSize) {
  if (byteBufferObj.is_null()) {
    return 0;
  }
  auto env = JvmEnv::get().getEnvironment();
  if (!env) {
    return 0;
  }
  auto bufferObj = byteBufferObj.obj();
  auto bufferData = (uint8_t*)env->GetDirectBufferAddress(bufferObj);
  auto bufferSize = (size_t)env->GetDirectBufferCapacity(bufferObj);
  if (bufferData && bufferSize > 0) {
    auto copySize = std::min(bufferSize, arrayByteSize);
    memcpy(array, bufferData, copySize);
    return copySize;
  } else {
    size_t copySize = 0;
    auto byteBuffer_class = env->FindClass("java/nio/ByteBuffer");
    auto byteBuffer_array = env->GetMethodID(byteBuffer_class, "array", "()[B");
    auto byteArray = (jbyteArray)env->CallObjectMethod(bufferObj, byteBuffer_array);
    if (byteArray != nullptr) {
      copySize = env->GetArrayLength(byteArray);
      auto arrayBuffer = env->GetByteArrayElements(byteArray, nullptr);
      memcpy(array, arrayBuffer, copySize);
      return copySize;
    }
    return copySize;
  }
  return 0;
}

void ByteBuffer::writeByteArray(const uint8_t *array, size_t arrayByteSize) {
  if (byteBufferObj.is_null()) {
    return;
  }
  auto env = JvmEnv::get().getEnvironment();
  if (!env) {
    return;
  }
  auto bufferObj = byteBufferObj.obj();
  auto bufferData = (uint8_t*)env->GetDirectBufferAddress(bufferObj);
  std::memcpy(bufferData, array, arrayByteSize);
}

void ByteBuffer::prepare(const JavaLocalRef<jclass>& clazzObj,
    const JavaLocalRef<jobject>& objRef) {
  clazz = clazzObj;
  byteBufferObj = objRef;
}

} // namespace jni
