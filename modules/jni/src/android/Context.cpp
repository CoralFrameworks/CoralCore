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

#include <core/jni/android/Context.h>

#include <core/jni/JniHelper.h>
#include <core/jni/JvmEnv.h>

#include <mutex>

namespace jni {

namespace {

constexpr const char* kClassName = "android/content/Context";

} // namespace anonymous

// ==================================================================================================
// Context
// ==================================================================================================

Context::Context(JNIEnv* env, const JavaRef<jobject>& context)
  : context_(env, context) {
  if (env == nullptr || context_.is_null()) {
    return;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass(kClassName));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return;
  }
  getApplicationContext_ = env->GetMethodID(clazz.obj(), "getApplicationContext",
      "()Landroid/content/Context;");
  getAssets_ = env->GetMethodID(clazz.obj(), "getAssets",
      "()Landroid/content/res/AssetManager;");
  getContentResolver_ = env->GetMethodID(clazz.obj(), "getContentResolver",
      "()Landroid/content/ContentResolver;");
  getResources_ = env->GetMethodID(clazz.obj(), "getResources",
      "()Landroid/content/res/Resources;");
  getPackageManager_ = env->GetMethodID(clazz.obj(), "getPackageManager",
      "()Landroid/content/pm/PackageManager;");
  getPackageName_ = env->GetMethodID(clazz.obj(), "getPackageName", "()Ljava/lang/String;");
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
  }
}

Context::~Context() = default;

std::unique_ptr<Context> Context::create(const jobject context) {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || context == nullptr) {
    return nullptr;
  }
  auto held = ScopedJavaLocalRef<jobject>(env, env->NewLocalRef(context));
  return std::unique_ptr<Context>(new Context(env, held));
}

std::unique_ptr<Context> Context::getApplicationContext() const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return nullptr;
  }
  auto object = ScopedJavaLocalRef<jobject>(env,
    env->CallObjectMethod(context_.obj(), getApplicationContext_));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (object.is_null()) {
    return nullptr;
  }
  return create(object.obj());
}

std::unique_ptr<AssetManager> Context::getAssets() const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || getAssets_ == nullptr || context_.is_null()) {
    return nullptr;
  }
  auto object = ScopedJavaLocalRef<jobject>(env, env->CallObjectMethod(context_.obj(), getAssets_));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (object.is_null()) {
    return nullptr;
  }
  return AssetManager::create(object.obj());
}

std::unique_ptr<ContentResolver> Context::getContentResolver() const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || getContentResolver_ == nullptr || context_.is_null()) {
    return nullptr;
  }
  auto object = ScopedJavaLocalRef<jobject>(env,
    env->CallObjectMethod(context_.obj(), getContentResolver_));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (object.is_null()) {
    return nullptr;
  }
  return ContentResolver::create(object.obj());
}

std::unique_ptr<Resources> Context::getResources() const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || getResources_ == nullptr || context_.is_null()) {
    return nullptr;
  }
  auto object = ScopedJavaLocalRef<jobject>(env,
    env->CallObjectMethod(context_.obj(), getResources_));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (object.is_null()) {
    return nullptr;
  }
  return Resources::create(object.obj());
}

std::unique_ptr<Resources> Context::getResourcesForApplication(
  const std::string& packageName) const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || getPackageManager_ == nullptr || context_.is_null()) {
    return nullptr;
  }
  auto packageManager = ScopedJavaLocalRef<jobject>(
    env, env->CallObjectMethod(context_.obj(), getPackageManager_));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (packageManager.is_null()) {
    return nullptr;
  }
  auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/content/pm/PackageManager"));
  if (clazz.is_null()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  jmethodID getResourcesForApplication = env->GetMethodID(clazz.obj(),
    "getResourcesForApplication",
    "(Ljava/lang/String;)Landroid/content/res/Resources;");
  if (getResourcesForApplication == nullptr) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  auto name = ScopedJavaLocalRef<jstring>(env, JniHelper::ConvertString(env, packageName));
  if (name.is_null()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  auto resources = ScopedJavaLocalRef<jobject>(env,
    env->CallObjectMethod(packageManager.obj(), getResourcesForApplication, name.obj()));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return nullptr;
  }
  if (resources.is_null()) {
    return nullptr;
  }
  return Resources::create(resources.obj());
}

std::string Context::getPackageName() const {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || getPackageName_ == nullptr || context_.is_null()) {
    return {};
  }
  auto name = ScopedJavaLocalRef<jstring>(env,
    static_cast<jstring>(env->CallObjectMethod(context_.obj(), getPackageName_)));
  if (env->ExceptionCheck()) {
    JniHelper::RecordJavaException(env);
    return {};
  }
  return JniHelper::ConvertUTFString(env, name.obj());
}

// ==================================================================================================
// ApplicationContext
// ==================================================================================================

namespace {

std::mutex gContextMutex;
jobject gApplicationContext = nullptr;

} // namespace anonymous

void ApplicationContext::initialize(const jobject context) {
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || context == nullptr) {
    return;
  }
  // The application context is the one that may be held for the lifetime of the process: a context
  // of an activity would keep the activity, and everything it holds, alive.
  jobject global = nullptr;
  {
    auto clazz = ScopedJavaLocalRef<jclass>(env, env->FindClass(kClassName));
    if (clazz.is_null()) {
      JniHelper::RecordJavaException(env);
      return;
    }
    const jmethodID getApplicationContext = env->GetMethodID(clazz.obj(),
        "getApplicationContext", "()Landroid/content/Context;");
    if (getApplicationContext == nullptr) {
      JniHelper::RecordJavaException(env);
      return;
    }
    auto resolved = ScopedJavaLocalRef<jobject>(env,
      env->CallObjectMethod(context, getApplicationContext));
    if (env->ExceptionCheck()) {
      JniHelper::RecordJavaException(env);
      return;
    }
    // The global reference is taken while the local reference of the application context is still
    // in scope: taking it after the block would read a reference that was already released.
    global = env->NewGlobalRef(resolved.is_null() ? context : resolved.obj());
  }
  if (global == nullptr) {
    JniHelper::RecordJavaException(env);
    return;
  }
  jobject previous = nullptr;
  {
    std::lock_guard<std::mutex> lock(gContextMutex);
    previous = gApplicationContext;
    gApplicationContext = global;
  }
  if (previous != nullptr) {
    env->DeleteGlobalRef(previous);
  }
}

bool ApplicationContext::isInitialized() {
  std::lock_guard<std::mutex> lock(gContextMutex);
  return gApplicationContext != nullptr;
}

std::unique_ptr<Context> ApplicationContext::get() {
  jobject context = nullptr;
  {
    std::lock_guard<std::mutex> lock(gContextMutex);
    context = gApplicationContext;
  }
  if (context == nullptr) {
    return nullptr;
  }
  return Context::create(context);
}

void ApplicationContext::release() {
  jobject previous = nullptr;
  {
    std::lock_guard<std::mutex> lock(gContextMutex);
    previous = gApplicationContext;
    gApplicationContext = nullptr;
  }
  if (previous == nullptr) {
    return;
  }
  JNIEnv* env = JvmEnv::getThreadEnvironment();
  if (env != nullptr) {
    env->DeleteGlobalRef(previous);
  }
}

} // namespace jni
