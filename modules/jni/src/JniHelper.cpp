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

#include <core/jni/JniHelper.h>
#include <core/jni/JniScopedRef.h>
#include <core/jni/JvmEnv.h>

#include <iostream>
#include <mutex>

namespace jni {

static std::mutex mutex_;

static jclass findClass(JNIEnv* env, const char* className) {
  return env->FindClass(className);
}

/*
 * Returns a human-readable summary of an exception object.  The buffer will
 * be populated with the "binary" class name and, if present, the
 * exception message.
 */
static bool getExceptionSummary(JNIEnv* env, jthrowable exception, std::string& result) {

  /* get the name of the exception's class */
  ScopedJavaLocalRef <jclass> exceptionClass(
      env, env->GetObjectClass(exception)); // can't fail
  ScopedJavaLocalRef <jclass> classClass(
      env,
      env->GetObjectClass(exceptionClass.obj())); // java.lang.Class, can't fail
  jmethodID classGetNameMethod = env->GetMethodID(classClass.obj(), "getName", "()Ljava/lang/String;");
  ScopedJavaLocalRef <jstring> classNameStr(
      env, (jstring)env->CallObjectMethod(exceptionClass.obj(), classGetNameMethod));
  if (classNameStr.obj() == nullptr) {
    env->ExceptionClear();
    result = "<error getting class name>";
    return false;
  }
  const char* classNameChars = env->GetStringUTFChars(classNameStr.obj(), nullptr);
  if (classNameChars == nullptr) {
    env->ExceptionClear();
    result = "<error getting class name UTF-8>";
    return false;
  }
  result += classNameChars;
  env->ReleaseStringUTFChars(classNameStr.obj(), classNameChars);

  /* if the exception has a detail message, get that */
  jmethodID getMessage = env->GetMethodID(exceptionClass.obj(), "getMessage", "()Ljava/lang/String;");
  ScopedJavaLocalRef <jstring> messageStr(
      env, (jstring)env->CallObjectMethod(exception, getMessage));
  if (messageStr.obj() == nullptr) {
    return true;
  }

  result += ": ";

  const char* messageChars = env->GetStringUTFChars(messageStr.obj(), nullptr);
  if (messageChars != nullptr) {
    result += messageChars;
    env->ReleaseStringUTFChars(messageStr.obj(), messageChars);
  } else {
    result += "<error getting message>";
    env->ExceptionClear(); // clear OOM
  }

  return true;
}

Helper* Helper::GetInstance() {
  static Helper helper;
  return &helper;
}

int Helper::ThrowException(JNIEnv* env, const char* className, const char* msg) {
  if (env->ExceptionCheck()) {
    /* TODO: consider creating the new exception with this as "cause" */
    ScopedJavaLocalRef <jthrowable> exception(env, env->ExceptionOccurred());
    env->ExceptionClear();

    if (exception.obj() != nullptr) {
      std::string text;
      getExceptionSummary(env, exception.obj(), text);
      std::cerr << "Discarding pending exception (" << text << ") to throw " << className << std::endl;
    }
  }

  ScopedJavaLocalRef <jclass> exceptionClass(env, findClass(env, className));
  if (exceptionClass.obj() == nullptr) {
    std::cerr << "Unable to find exception class " << className << std::endl;
    /* ClassNotFoundException now pending */
    return -1;
  }

  if (env->ThrowNew(exceptionClass.obj(), msg) != JNI_OK) {
    std::cerr << "Failed throwing '" << className << "' '" << msg << "'" << std::endl;
    /* an exception, most likely OOM, will now be pending */
    return -1;
  }

  return 0;
}

void Helper::DetachCurrentThread() {
  auto vm = JvmEnv::get().getVirtualMachine();
  if (vm) {
    vm->DetachCurrentThread();
  }
}

jstring Helper::ConvertString(JNIEnv* env, const std::string& str) {
  jstring result = nullptr;
  if (env == nullptr) {
    return result;
  }

  const auto* cStr = str.c_str();

  int size = strlen(cStr);

  jbyteArray array = nullptr;
  array = env->NewByteArray(size);
  if (array) {
    jbyte* bytes = env->GetByteArrayElements(array, nullptr);
    if (bytes != nullptr) {
      memcpy(bytes, cStr, size);
      env->ReleaseByteArrayElements(array, bytes, 0);

      jclass clazz = env->FindClass("java/lang/String");
      if (clazz == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(array);
        return nullptr;
      }
      jmethodID pMethodID = env->GetMethodID(clazz, "<init>", "([BLjava/lang/String;)V");
      jstring utf = env->NewStringUTF("UTF-8");
      if (pMethodID == nullptr || utf == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(clazz);
        if (utf != nullptr) {
          env->DeleteLocalRef(utf);
        }
        env->DeleteLocalRef(array);
        return nullptr;
      }
      result = (jstring)env->NewObject(clazz, pMethodID, array, utf);

      // The class reference belongs to the caller's frame only: releasing it here keeps the
      // local reference table from growing on the audio threads that call this per buffer.
      env->DeleteLocalRef(clazz);
      env->DeleteLocalRef(utf);
    }
  }
  env->DeleteLocalRef(array);

  return result;
}

std::string Helper::ConvertUTFString(JNIEnv* env, jstring str) {
  if (env == nullptr || str == nullptr) {
    return {};
  }
  const char* charStr = env->GetStringUTFChars(str, 0);
  if (charStr == nullptr) {
    // Out of memory: the JNI contract leaves an exception pending, which the caller clears.
    return {};
  }
  std::string result(charStr);
  env->ReleaseStringUTFChars(str, charStr);
  return result;
}

std::string Helper::DescribeException(JNIEnv* env) {
  if (env == nullptr || env->ExceptionCheck() != JNI_TRUE) {
    return {};
  }

  // The throwable is taken and the exception is cleared before anything else is called, so
  // the calls below run on an environment that has no pending exception of its own.
  const auto throwable = env->ExceptionOccurred();
  env->ExceptionClear();
  if (throwable == nullptr) {
    return {};
  }

  std::string description;
  const auto throwableClass = env->FindClass("java/lang/Throwable");
  if (throwableClass != nullptr) {
    const auto toString = env->GetMethodID(throwableClass, "toString", "()Ljava/lang/String;");
    if (toString != nullptr) {
      const auto text = static_cast<jstring>(env->CallObjectMethod(throwable, toString));
      if (env->ExceptionCheck() == JNI_TRUE) {
        env->ExceptionClear();
      } else if (text != nullptr) {
        description = ConvertUTFString(env, text);
        env->DeleteLocalRef(text);
      }
    }
    env->DeleteLocalRef(throwableClass);
  }
  env->DeleteLocalRef(throwable);
  return description;
}

namespace {
/// The description of the last exception the wrappers of this thread reported.
thread_local std::string gLastError;
} // namespace anonymous

void Helper::RecordJavaException(JNIEnv* env) noexcept {
  gLastError = DescribeException(env);
}

std::string Helper::takeLastError() {
  std::string error;
  error.swap(gLastError);
  return error;
}

int Helper::ThrowException(const char* className, const char* msg) {
  auto vm = JvmEnv::get().getVirtualMachine();
  if (!vm) {
    return -1;
  }
  int result = 0;
  JNIEnv* env;

  std::unique_lock <std::mutex> lock(mutex_);
#if defined(__ANDROID__)
  vm->AttachCurrentThread(&env, nullptr);
#else
  vm->AttachCurrentThread(reinterpret_cast<void **>(&env), nullptr);
#endif
  result = ThrowException(env, className, msg);
  vm->DetachCurrentThread();
  lock.unlock();

  return result;
}

} // namespace jni
