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

#include <core/jni/android/Resources.h>

#include <core/jni/JniHelper.h>
#include <core/jni/JvmEnv.h>

namespace jni {

namespace {
constexpr const char* kClassName = "android/content/res/Resources";
} // namespace anonymous

Resources::Resources(JNIEnv* env, const JavaRef<jobject>& resources)
  : resources_(env, resources) {
  if (env == nullptr || resources_.is_null()) {
    return;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass(kClassName));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  openRawResourceFd_ = env->GetMethodID(clazz.obj(), "openRawResourceFd",
      "(I)Landroid/content/res/AssetFileDescriptor;");
  getIdentifier_ = env->GetMethodID(clazz.obj(), "getIdentifier",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
  }
}

Resources::~Resources() = default;

std::unique_ptr<Resources> Resources::create(const jobject resources) {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || resources == nullptr) {
    return nullptr;
  }
  auto held = ScopedJavaLocalRef<jobject>(env, env->NewLocalRef(resources));
  return std::unique_ptr<Resources>(new Resources(env, held));
}

std::unique_ptr<AssetFileDescriptor> Resources::openRawResourceFd(const int resourceId) const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || openRawResourceFd_ == nullptr || resources_.is_null()) {
    return nullptr;
  }
  auto descriptor = ScopedJavaLocalRef<jobject>(
    env, env->CallObjectMethod(resources_.obj(), openRawResourceFd_, static_cast<jint>(resourceId)));
  if (env->ExceptionCheck()) {
    // A missing or compressed resource is a refusal of the platform rather than a failure of the
    // call: the caller answers it with a failure of its own.
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (descriptor.is_null()) {
    return nullptr;
  }
  return AssetFileDescriptor::create(descriptor.obj());
}

int Resources::getIdentifier(const std::string& name, const std::string& defType,
                             const std::string& defPackage) const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || getIdentifier_ == nullptr || resources_.is_null()) {
    return 0;
  }
  auto nameText = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, name));
  // An empty type or package means "the one of the name", which the platform spells as null.
  auto typeText = defType.empty()
    ? ScopedJavaLocalRef<jstring>(env, static_cast<jstring>(nullptr))
    : ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, defType));
  auto packageText = defPackage.empty()
    ? ScopedJavaLocalRef<jstring>(env, static_cast<jstring>(nullptr))
    : ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, defPackage));
  if (nameText.is_null()) {
    JniHelper::RecordJavaException(env);
    return 0;
  }
  const jint identifier = env->CallIntMethod(resources_.obj(), getIdentifier_, nameText.obj(),
    typeText.obj(), packageText.obj());
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return 0;
  }
  return static_cast<int>(identifier);
}

} // namespace jni
