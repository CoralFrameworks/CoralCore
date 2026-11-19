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

#include <core/jni/android/AssetManager.h>

#include <core/jni/JniHelper.h>
#include <core/jni/JvmEnv.h>

namespace jni {

namespace {
constexpr const char* kClassName = "android/content/res/AssetManager";
} // namespace anonymous

AssetManager::AssetManager(JNIEnv* env, const JavaRef<jobject>& assetManager)
  : manager_(env, assetManager) {
  if (env == nullptr || manager_.is_null()) {
    return;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass(kClassName));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  open_ = env->GetMethodID(clazz.obj(), "open", "(Ljava/lang/String;I)Ljava/io/InputStream;");
  openFd_ = env->GetMethodID(clazz.obj(), "openFd",
      "(Ljava/lang/String;)Landroid/content/res/AssetFileDescriptor;");
  list_ = env->GetMethodID(clazz.obj(), "list", "(Ljava/lang/String;)[Ljava/lang/String;");
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
  }
}

AssetManager::~AssetManager() = default;

std::unique_ptr<AssetManager> AssetManager::create(const jobject assetManager) {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || assetManager == nullptr) {
    return nullptr;
  }
  auto held = ScopedJavaLocalRef<jobject>(env, env->NewLocalRef(assetManager));
  return std::unique_ptr<AssetManager>(new AssetManager(env, held));
}

std::unique_ptr<InputStream> AssetManager::open(const std::string& path,
                                                const AccessMode mode) const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || open_ == nullptr || manager_.is_null()) {
    return nullptr;
  }
  auto name = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, path));
  if (name.is_null()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  auto stream = ScopedJavaLocalRef<jobject>(env,
    env->CallObjectMethod(manager_.obj(), open_, name.obj(), static_cast<jint>(mode)));
  if (env->ExceptionCheck()) {
    // A missing asset is a refusal of the platform rather than a failure of the call: it is
    // recorded for the caller to log, and null is what it has to deal with either way.
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (stream.is_null()) {
    return nullptr;
  }
  return InputStream::create(stream.obj());
}

std::unique_ptr<AssetFileDescriptor> AssetManager::openFd(const std::string& path) const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || openFd_ == nullptr || manager_.is_null()) {
    return nullptr;
  }
  auto name = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, path));
  if (name.is_null()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  auto descriptor = ScopedJavaLocalRef<jobject>(
    env, env->CallObjectMethod(manager_.obj(), openFd_, name.obj()));
  if (env->ExceptionCheck()) {
    // The platform refuses an asset that is stored compressed with a FileNotFoundException, which
    // the caller answers by reading the asset as a stream instead.
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (descriptor.is_null()) {
    return nullptr;
  }
  return AssetFileDescriptor::create(descriptor.obj());
}

std::vector<std::string> AssetManager::list(const std::string& path) const {
  std::vector<std::string> entries;
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || list_ == nullptr || manager_.is_null()) {
    return entries;
  }
  auto name = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, path));
  if (name.is_null()) {
    JniHelper::RecordJavaException(env);
    return entries;
  }
  auto array = ScopedJavaLocalRef<jobjectArray>(
    env, static_cast<jobjectArray>(env->CallObjectMethod(manager_.obj(), list_, name.obj())));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return entries;
  }
  if (array.is_null()) {
    return entries;
  }
  const jsize count = env->GetArrayLength(array.obj());
  entries.reserve(static_cast<size_t>(count));
  for (jsize index = 0; index < count; ++index) {
    auto entry = ScopedJavaLocalRef<jstring>(
      env, static_cast<jstring>(env->GetObjectArrayElement(array.obj(), index)));
    entries.push_back(JniHelper::ConvertUTFString(env, entry.obj()));
  }
  return entries;
}

} // namespace jni
