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

#include <core/jni/android/ContentResolver.h>

#include <core/jni/JniHelper.h>
#include <core/jni/JvmEnv.h>
#include <core/jni/android/Uri.h>

namespace jni {

namespace {

constexpr const char* kClassName = "android/content/ContentResolver";

/**
 * The key of the option that asks a media provider for the original media rather than a copy it
 * transcoded, the value of {@code MediaStore.EXTRA_ACCEPT_ORIGINAL_MEDIA_FORMAT}.
 */
constexpr const char* kAcceptOriginalMediaFormat =
    "android.provider.extra.ACCEPT_ORIGINAL_MEDIA_FORMAT";

/**
 * Builds the options of {@link ContentResolver::openTypedAssetFileDescriptor}.
 *
 * @return the bundle, or null when the platform refused to create it.
 */
ScopedJavaLocalRef<jobject> createProviderOptions(JNIEnv* env) {
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/os/Bundle"));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return ScopedJavaLocalRef<jobject>(env, static_cast<jobject>(nullptr));
  }
  jmethodID constructor = env->GetMethodID(clazz.obj(), "<init>", "()V");
  jmethodID putBoolean = env->GetMethodID(clazz.obj(), "putBoolean", "(Ljava/lang/String;Z)V");
  if (constructor == nullptr || putBoolean == nullptr) {
    JniHelper::RecordJavaException(env);
    return ScopedJavaLocalRef<jobject>(env, static_cast<jobject>(nullptr));
  }
  auto options = ScopedJavaLocalRef<jobject>(env, env->NewObject(clazz.obj(), constructor));
  if (options.is_null()) {
    JniHelper::RecordJavaException(env);
    return options;
  }
  auto key = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, kAcceptOriginalMediaFormat));
  if (key.is_null()) {
    JniHelper::RecordJavaException(env);
    return ScopedJavaLocalRef<jobject>(env, static_cast<jobject>(nullptr));
  }
  env->CallVoidMethod(options.obj(), putBoolean, key.obj(), JNI_TRUE);
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return ScopedJavaLocalRef<jobject>(env, static_cast<jobject>(nullptr));
  }
  return options;
}

} // namespace anonymous

ContentResolver::ContentResolver(JNIEnv* env, const JavaRef<jobject>& contentResolver)
  : resolver_(env, contentResolver) {
  if (env == nullptr || resolver_.is_null()) {
    return;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass(kClassName));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  openAssetFileDescriptor_ = env->GetMethodID(clazz.obj(), "openAssetFileDescriptor",
      "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/res/AssetFileDescriptor;");
  openTypedAssetFileDescriptor_ = env->GetMethodID(clazz.obj(), "openTypedAssetFileDescriptor",
      "(Landroid/net/Uri;Ljava/lang/String;Landroid/os/Bundle;)"
      "Landroid/content/res/AssetFileDescriptor;");
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
  }
}

ContentResolver::~ContentResolver() = default;

std::unique_ptr<ContentResolver> ContentResolver::create(const jobject contentResolver) {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || contentResolver == nullptr) {
    return nullptr;
  }
  auto held = ScopedJavaLocalRef<jobject>(env, env->NewLocalRef(contentResolver));
  return std::unique_ptr<ContentResolver>(new ContentResolver(env, held));
}

std::unique_ptr<AssetFileDescriptor> ContentResolver::openAssetFileDescriptor(
  const std::string& uri, const std::string& mode) const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || openAssetFileDescriptor_ == nullptr || resolver_.is_null()) {
    return nullptr;
  }
  auto parsed = Uri::parse(uri);
  if (parsed == nullptr || parsed->isNull()) {
    return nullptr;
  }
  auto modeText = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, mode));
  if (modeText.is_null()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  auto descriptor = ScopedJavaLocalRef<jobject>(env,
    env->CallObjectMethod(resolver_.obj(), openAssetFileDescriptor_, parsed->obj(), modeText.obj()));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (descriptor.is_null()) {
    return nullptr;
  }
  return AssetFileDescriptor::create(descriptor.obj());
}

std::unique_ptr<AssetFileDescriptor> ContentResolver::openTypedAssetFileDescriptor(
  const std::string& uri, const std::string& mimeType) const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || openTypedAssetFileDescriptor_ == nullptr || resolver_.is_null()) {
    return nullptr;
  }
  auto parsed = Uri::parse(uri);
  if (parsed == nullptr || parsed->isNull()) {
    return nullptr;
  }
  auto type = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, mimeType));
  if (type.is_null()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  auto options = createProviderOptions(env);
  auto descriptor = ScopedJavaLocalRef<jobject>(env,
    env->CallObjectMethod(resolver_.obj(), openTypedAssetFileDescriptor_, parsed->obj(), type.obj(),
      options.obj()));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (descriptor.is_null()) {
    return nullptr;
  }
  return AssetFileDescriptor::create(descriptor.obj());
}

} // namespace jni
