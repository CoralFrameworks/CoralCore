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

#ifndef CORAL_JNI_JVM_ENV_H
#define CORAL_JNI_JVM_ENV_H

#include <jni.h>

#include <assert.h>

namespace jni {

class VirtualMachineEnv {
public:
  /**
   * This is called when the library is loaded. We need this to get a reference to the global VM
   */
  static jint jniLoad(JavaVM* vm) noexcept;

  static void jniUnload(JavaVM* vm, void* reserved) noexcept;

  static VirtualMachineEnv& get() noexcept {
    // declaring this thread local, will ensure it's destroyed with the calling thread
    static thread_local VirtualMachineEnv instance;
    return instance;
  }

  static JNIEnv* getThreadEnvironment() noexcept {
    JNIEnv* env;
    if (sVirtualMachine == nullptr) {
      // The virtual machine was never set, or the library was unloaded: a null environment is
      // reported instead of dereferencing a null pointer in release builds.
      return nullptr;
    }
    auto state = sVirtualMachine->GetEnv(reinterpret_cast<void**>(&env),
        JNI_VERSION_1_6);
    if (state == JNI_EDETACHED) {
#if defined(__ANDROID__)
      sVirtualMachine->AttachCurrentThread(&env, nullptr);
#else
      sVirtualMachine->AttachCurrentThread(reinterpret_cast<void **>(&env), nullptr);
#endif
    } else if (state != JNI_OK) {
      return nullptr; // this should not happen
    }
    return env;
  }

  static void handleException(JNIEnv* env) noexcept;

  inline JNIEnv* getEnvironment() noexcept {
    // This is called on the main thread
    if (mVirtualMachine == nullptr) {
      return nullptr;
    }
    JNIEnv* env = mJniEnv;
    if (!env) {
      return getEnvironmentSlow();
    }
    return env;
  }

  JNIEnv* getEnvironmentSlow() noexcept;

private:
  VirtualMachineEnv() noexcept: mVirtualMachine(sVirtualMachine) {
    // We're not initializing the JVM here -- but we could -- because most of
    // the time we don't need the jvm. Instead we do the initialization on first
    // use. This means we could get a nasty slow down the very first time, but
    // we'll live with it for now.
  }

  ~VirtualMachineEnv() {
    if (mVirtualMachine) {
      mVirtualMachine->DetachCurrentThread();
    }
  }

  JavaVM* getVirtualMachine() noexcept { return mVirtualMachine; }

  static JavaVM* sVirtualMachine;
  JNIEnv* mJniEnv = nullptr;
  JavaVM* mVirtualMachine = nullptr;

  friend class Helper;
};

} // namespace jni

using JvmEnv = jni::VirtualMachineEnv;

#endif // CORAL_JNI_JVM_ENV_H
