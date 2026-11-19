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

#include <core/jni/JvmEnv.h>

namespace jni {

JavaVM* VirtualMachineEnv::sVirtualMachine = nullptr;

jint VirtualMachineEnv::jniLoad(JavaVM* vm) noexcept {
  JNIEnv* env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    // this should not happen
    return -1;
  }
  sVirtualMachine = vm;
  return JNI_VERSION_1_6;
}

void VirtualMachineEnv::jniUnload(JavaVM* vm, void* reserved) noexcept {
  if (vm == sVirtualMachine) {
    sVirtualMachine = nullptr;
  }
}

void VirtualMachineEnv::handleException(JNIEnv* const env) noexcept {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
  }
}

JNIEnv* VirtualMachineEnv::getEnvironmentSlow() noexcept {
#if defined(__ANDROID__)
  mVirtualMachine->AttachCurrentThread(&mJniEnv, nullptr);
#else
  mVirtualMachine->AttachCurrentThread(reinterpret_cast<void **>(&mJniEnv), nullptr);
#endif
  assert(mJniEnv);
  return mJniEnv;
}

} // namespace jni
