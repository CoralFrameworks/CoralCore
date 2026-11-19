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

#include <core/jni/android/Uri.h>

#include <core/jni/JniHelper.h>
#include <core/jni/JvmEnv.h>

namespace jni {

Uri::Uri(JNIEnv* env, const JavaRef<jobject>& uri)
  : uri_(env, uri) {
  if (env == nullptr || uri_.is_null()) {
    return;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/net/Uri"));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  uriClass_ = clazz;
  getScheme_ = env->GetMethodID(clazz.obj(), "getScheme", "()Ljava/lang/String;");
  getPath_ = env->GetMethodID(clazz.obj(), "getPath", "()Ljava/lang/String;");
  getAuthority_ = env->GetMethodID(clazz.obj(), "getAuthority", "()Ljava/lang/String;");
  getHost_ = env->GetMethodID(clazz.obj(), "getHost", "()Ljava/lang/String;");
  getLastPathSegment_ =
    env->GetMethodID(clazz.obj(), "getLastPathSegment", "()Ljava/lang/String;");
  getPathSegments_ = env->GetMethodID(clazz.obj(), "getPathSegments", "()Ljava/util/List;");
  getQueryParameter_ =
    env->GetMethodID(clazz.obj(), "getQueryParameter", "(Ljava/lang/String;)Ljava/lang/String;");
  toString_ = env->GetMethodID(clazz.obj(), "toString", "()Ljava/lang/String;");
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
  }
}

Uri::~Uri() = default;

std::unique_ptr<Uri> Uri::parse(const std::string& uri) {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return nullptr;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/net/Uri"));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  jmethodID parse = env->GetStaticMethodID(clazz.obj(), "parse",
      "(Ljava/lang/String;)Landroid/net/Uri;");
  if (parse == nullptr) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  auto text = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, uri));
  if (text.is_null()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  auto object = ScopedJavaLocalRef<jobject>(
    env, env->CallStaticObjectMethod(clazz.obj(), parse, text.obj()));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (object.is_null()) {
    return nullptr;
  }
  return std::unique_ptr<Uri>(new Uri(env, object));
}

std::string Uri::callString(const jmethodID method) const {
  if (method == nullptr || uri_.is_null()) {
    return {};
  }
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return {};
  }
  auto text = ScopedJavaLocalRef<jstring>(env,
    static_cast<jstring>(env->CallObjectMethod(uri_.obj(), method)));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return {};
  }
  return JniHelper::ConvertUTFString(env, text.obj());
}

std::string Uri::getScheme() const {
  return callString(getScheme_);
}

std::string Uri::getPath() const {
  return callString(getPath_);
}

std::string Uri::getAuthority() const {
  return callString(getAuthority_);
}

std::string Uri::getHost() const {
  return callString(getHost_);
}

std::string Uri::getLastPathSegment() const {
  return callString(getLastPathSegment_);
}

std::vector<std::string> Uri::getPathSegments() const {
  std::vector<std::string> segments;
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (getPathSegments_ == nullptr || env == nullptr || uri_.is_null()) {
    return segments;
  }
  auto list = ScopedJavaLocalRef<jobject>(env, env->CallObjectMethod(uri_.obj(), getPathSegments_));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return segments;
  }
  if (list.is_null()) {
    return segments;
  }
  auto listClass = ScopedJavaLocalRef<jclass>(env, env->FindClass("java/util/List"));
  if (listClass.is_null()) {
    JniHelper::RecordJavaException(env);
    return segments;
  }
  jmethodID size = env->GetMethodID(listClass.obj(), "size", "()I");
  jmethodID get = env->GetMethodID(listClass.obj(), "get", "(I)Ljava/lang/Object;");
  if (size == nullptr || get == nullptr) {
    JniHelper::RecordJavaException(env);
    return segments;
  }
  const jint count = env->CallIntMethod(list.obj(), size);
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return segments;
  }
  segments.reserve(count > 0 ? static_cast<size_t>(count) : 0);
  for (jint index = 0; index < count; ++index) {
    auto item = ScopedJavaLocalRef<jstring>(env,
      static_cast<jstring>(env->CallObjectMethod(list.obj(), get, index)));
    if (env->ExceptionCheck()) {
      JniHelper::RecordJavaException(env);
      break;
    }
    segments.push_back(JniHelper::ConvertUTFString(env, item.obj()));
  }
  return segments;
}

std::string Uri::getQueryParameter(const std::string& key) const {
  if (getQueryParameter_ == nullptr || uri_.is_null()) {
    return {};
  }
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return {};
  }
  auto name = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, key));
  if (name.is_null()) {
    JniHelper::RecordJavaException(env);
    return {};
  }
  auto text = ScopedJavaLocalRef<jstring>(env,
    static_cast<jstring>(env->CallObjectMethod(uri_.obj(), getQueryParameter_, name.obj())));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return {};
  }
  return JniHelper::ConvertUTFString(env, text.obj());
}

std::string Uri::toString() const {
  return callString(toString_);
}

} // namespace jni
