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

#include <core/jni/android/Surface.h>

namespace jni {

std::unique_ptr<Surface> Surface::fromObject(JNIEnv* env, jobject obj) {
  if ((env == nullptr) || (obj == nullptr)) {
    return nullptr;
  }
  auto surface = std::make_unique<Surface>();
  surface->bind(env);
  auto surfaceObj = ScopedJavaLocalRef<jobject>(env, obj);
  surface->surface = surfaceObj;
  return surface;
}

void* Surface::get() const { return surface.obj(); }

bool Surface::isValid() const {
  if (surface.is_null()) {
    return false;
  }
  auto *env = JvmEnv::get().getEnvironment();
  if (env == nullptr) {
    return false;
  }
  JvmEnv::handleException(env);
  return env->CallBooleanMethod(surface.obj(), surface_isValid);
}

void Surface::release() {
  auto* env = JvmEnv::get().getEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);
  env->CallVoidMethod(surface.obj(), surface_release);
}

void Surface::bind(JNIEnv* env) {
  JvmEnv::handleException(env);

  auto classObj = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/view/Surface"));
  surface_class = classObj;

  surface_isValid = env->GetMethodID(surface_class.obj(), "isValid", "()Z");
  surface_release = env->GetMethodID(surface_class.obj(), "release", "()V");
}

} // namespace jni
