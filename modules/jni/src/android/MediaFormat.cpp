/*
 * Copyright (C) 2022 The Graphifx Project
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

#include <core/jni/android/MediaFormat.h>

#include <core/jni/JniHelper.h>
#include <core/jni/JvmEnv.h>

namespace jni {

std::unique_ptr<MediaFormat> MediaFormat::create() {
  return std::make_unique<MediaFormat>();
}

std::unique_ptr<MediaFormat>
MediaFormat::create(const ScopedJavaLocalRef<jobject> &object) {
  return std::make_unique<MediaFormat>(object);
}

MediaFormat::MediaFormat() { bind(); }

MediaFormat::MediaFormat(const ScopedJavaLocalRef<jobject> &object) noexcept {
  this->object = object;
  bind();
}

void MediaFormat::bind() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);

  auto clazzRef = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/media/MediaFormat"));
  clazz = clazzRef;

  init_id = env->GetMethodID(clazz.obj(), "<init>", "()V");
  contains_key_id = env->GetMethodID(clazz.obj(), "containsKey", "(Ljava/lang/String;)Z");

  get_integer_id = env->GetMethodID(clazz.obj(), "getInteger", "(Ljava/lang/String;)I");
  get_long_id = env->GetMethodID(clazz.obj(), "getLong", "(Ljava/lang/String;)J");
  get_float_id = env->GetMethodID(clazz.obj(), "getFloat", "(Ljava/lang/String;)F");
  get_bytebuffer_id =
      env->GetMethodID(clazz.obj(), "getByteBuffer", "(Ljava/lang/String;)Ljava/nio/ByteBuffer;");
  get_string_id =
      env->GetMethodID(clazz.obj(), "getString", "(Ljava/lang/String;)Ljava/lang/String;");

  set_integer_id = env->GetMethodID(clazz.obj(), "setInteger", "(Ljava/lang/String;I)V");
  set_long_id = env->GetMethodID(clazz.obj(), "setLong", "(Ljava/lang/String;J)V");
  set_float_id = env->GetMethodID(clazz.obj(), "setFloat", "(Ljava/lang/String;F)V");
  set_bytebuffer_id =
      env->GetMethodID(clazz.obj(), "setByteBuffer", "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V");
  set_string_id =
      env->GetMethodID(clazz.obj(), "setString", "(Ljava/lang/String;Ljava/lang/String;)V");

  to_string_id = env->GetMethodID(clazz.obj(), "toString", "()Ljava/lang/String;");

  if (object.is_null()) {
    auto formatRef = ScopedJavaLocalRef<jobject>(env, env->NewObject(clazz.obj(), init_id));
    object = formatRef;
  }

  prepared = true;
}

std::string MediaFormat::toString() {
  if (!prepared || object.is_null()) {
    return {};
  }

  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return {};
  }
  JvmEnv::handleException(env);

  auto *description =
      (jstring) env->CallObjectMethod(object.obj(), to_string_id);
  if (description != nullptr) {
    return JniHelper::ConvertUTFString(env, description);
  }
  return {};
}

bool MediaFormat::getInteger(const std::string &key, int32_t *out) {
  if (!prepared || object.is_null()) {
    return false;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return false;
  }
  JvmEnv::handleException(env);

  auto *keyStr = JniHelper::ConvertString(env, key);
  if (keyStr == nullptr) {
    return false;
  }
  auto containKeys =
      env->CallBooleanMethod(object.obj(), contains_key_id, keyStr);
  if (!containKeys) {
    env->DeleteLocalRef(keyStr);
    return false;
  }
  *out = env->CallIntMethod(object.obj(), get_integer_id, keyStr);
  env->DeleteLocalRef(keyStr);
  if (env->ExceptionCheck()) {
    JvmEnv::handleException(env);
    return false;
  }
  return true;
}

bool MediaFormat::getLong(const std::string &key, int64_t *out) {
  if (!prepared || object.is_null()) {
    return false;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return false;
  }
  JvmEnv::handleException(env);

  auto *keyStr = JniHelper::ConvertString(env, key);
  if (keyStr == nullptr) {
    return false;
  }
  auto containKeys =
      env->CallBooleanMethod(object.obj(), contains_key_id, keyStr);
  if (!containKeys) {
    env->DeleteLocalRef(keyStr);
    return false;
  }
  *out = env->CallLongMethod(object.obj(), get_long_id, keyStr);
  env->DeleteLocalRef(keyStr);
  if (env->ExceptionCheck()) {
    JvmEnv::handleException(env);
    return false;
  }
  return true;
}

bool MediaFormat::getFloat(const std::string &key, float *out) {
  if (!prepared || object.is_null()) {
    return false;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return false;
  }
  JvmEnv::handleException(env);
  auto *keyStr = JniHelper::ConvertString(env, key);
  if (keyStr == nullptr) {
    return false;
  }
  auto containKeys =
      env->CallBooleanMethod(object.obj(), contains_key_id, keyStr);
  if (!containKeys) {
    env->DeleteLocalRef(keyStr);
    return false;
  }
  *out = env->CallFloatMethod(object.obj(), get_float_id, keyStr);
  env->DeleteLocalRef(keyStr);
  if (env->ExceptionCheck()) {
    JvmEnv::handleException(env);
    return false;
  } else {
    return true;
  }
}

std::unique_ptr<JniByteBuffer> MediaFormat::getByteBuffer(const std::string &key) {
  jobject bufferObj = nullptr;
  if (!prepared || object.is_null()) {
    return nullptr;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return nullptr;
  }
  JvmEnv::handleException(env);

  auto keyStr = JniHelper::ConvertString(env, key);
  if (!keyStr) {
    return nullptr;
  }
  auto containKeys =
      env->CallBooleanMethod(object.obj(), contains_key_id, keyStr);
  if (!containKeys) {
    env->DeleteLocalRef(keyStr);
    return nullptr;
  }

  auto ret = true;
  bufferObj = env->CallObjectMethod(object.obj(), get_bytebuffer_id, keyStr);
  env->DeleteLocalRef(keyStr);
  if (bufferObj != nullptr) {
    return JniByteBuffer::createWithObject(env, bufferObj);
  }

  return nullptr;
}

bool MediaFormat::getString(const std::string &key, std::string *out) {
  jobject result = nullptr;
  if (!prepared || object.is_null()) {
    return false;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return false;
  }
  JvmEnv::handleException(env);

  auto *keyStr = JniHelper::ConvertString(env, key);
  if (keyStr == nullptr) {
    return false;
  }
  auto containKeys =
      env->CallBooleanMethod(object.obj(), contains_key_id, keyStr);
  if (!containKeys) {
    env->DeleteLocalRef(keyStr);
    return false;
  }

  auto ret = true;
  result = env->CallObjectMethod(object.obj(), get_string_id, keyStr);
  env->DeleteLocalRef(keyStr);
  if (env->ExceptionCheck()) {
    JvmEnv::handleException(env);
    ret = false;
  } else {
    *out = JniHelper::ConvertUTFString(env, (jstring) result);
    if (out->empty()) {
      ret = false;
    }
  }

  if (result != nullptr) {
    env->DeleteLocalRef(result);
  }

  return ret;
}

void MediaFormat::setInteger(const std::string &key, int32_t value) {
  if (!prepared || object.is_null()) {
    return;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);

  auto *keyStr = JniHelper::ConvertString(env, key);
  if (keyStr == nullptr) {
    return;
  }
  env->CallVoidMethod(object.obj(), set_integer_id, keyStr, value);
  env->DeleteLocalRef(keyStr);
  JvmEnv::handleException(env);
}

void MediaFormat::setLong(const std::string &key, int64_t value) {
  if (!prepared || object.is_null()) {
    return;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);

  auto *keyStr = JniHelper::ConvertString(env, key);
  if (keyStr == nullptr) {
    return;
  }
  env->CallVoidMethod(object.obj(), set_long_id, keyStr, value);
  env->DeleteLocalRef(keyStr);
  JvmEnv::handleException(env);
}

void MediaFormat::setFloat(const std::string &key, float value) {
  if (!prepared || object.is_null()) {
    return;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);

  auto *keyStr = JniHelper::ConvertString(env, key);
  if (keyStr == nullptr) {
    return;
  }
  env->CallVoidMethod(object.obj(), set_float_id, keyStr, value);
  env->DeleteLocalRef(keyStr);
  JvmEnv::handleException(env);
}

void MediaFormat::setByteBuffer(const std::string &key, void *data, size_t size) {
  if (!prepared || object.is_null() || !data || size <= 0) {
    return;
  }

  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);

  auto *keyStr = JniHelper::ConvertString(env, key);
  if (keyStr == nullptr) {
    return;
  }

  auto byteBuffer_class = env->FindClass("java/nio/ByteBuffer");
  auto mid_wrap =
      env->GetStaticMethodID(byteBuffer_class, "wrap", "([B)Ljava/nio/ByteBuffer;");
  auto buffer = env->NewByteArray((jsize) size);
  env->SetByteArrayRegion(buffer, 0, (jsize) size, reinterpret_cast<const jbyte *>(data));
  auto byteBufferObj =
      env->CallStaticObjectMethod(byteBuffer_class, mid_wrap, buffer);
  if (byteBufferObj != nullptr) {
    env->CallVoidMethod(object.obj(), set_bytebuffer_id, keyStr, byteBufferObj);
  }

  env->DeleteLocalRef(keyStr);
  JvmEnv::handleException(env);
}

void MediaFormat::setString(const std::string &key, const std::string &value) {
  jstring valurStr = nullptr;
  if (!prepared || object.is_null()) {
    return;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);

  auto *keyStr = JniHelper::ConvertString(env, key);
  if (keyStr == nullptr) {
    return;
  }
  valurStr = JniHelper::ConvertString(env, value);
  if (valurStr == nullptr) {
    env->DeleteLocalRef(keyStr);
    return;
  }
  env->CallVoidMethod(object.obj(), set_string_id, keyStr, valurStr);
  env->DeleteLocalRef(keyStr);
  env->DeleteLocalRef(valurStr);
  JvmEnv::handleException(env);
}

} // namespace jni
