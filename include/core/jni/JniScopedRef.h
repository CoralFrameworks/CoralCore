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

#ifndef CORAL_JNI_SCOPED_JAVA_REF_H
#define CORAL_JNI_SCOPED_JAVA_REF_H

#include <core/jni/JvmEnv.h>

#include <utility>

namespace jni {

template<typename T>
class JavaRef;

template<>
class JavaRef<jobject> {
public:
  JavaRef(const JavaRef&) = delete;
  JavaRef& operator=(const JavaRef&) = delete;

  [[nodiscard]] jobject obj() const { return obj_; }

  [[nodiscard]] bool is_null() const { return obj_ == nullptr; }

protected:
  constexpr JavaRef() : obj_(nullptr) {}

  explicit JavaRef(jobject obj) : obj_(obj) {}

  jobject obj_;
};

template<typename T>
class JavaRef : public JavaRef<jobject> {
public:
  JavaRef(const JavaRef&) = delete;
  JavaRef& operator=(const JavaRef&) = delete;

  T obj() const { return static_cast<T>(obj_); }

protected:
  JavaRef() : JavaRef<jobject>(nullptr) {}

  explicit JavaRef(T obj) noexcept: JavaRef<jobject>(obj) {}
};

/**
 * Holds a local reference to a Java object. The local reference is scoped
 * to the lifetime of this object.
 * Instances of this class may hold onto any JNIEnv passed into it until
 * destroyed. Therefore, since a JNIEnv is only suitable for use on a single
 * thread, objects of this class must be created, used, and destroyed, on a
 * single thread.
 * Therefore, this class should only be used as a stack-based object and from a
 * single thread. If you wish to have the reference outlive the current
 * callstack (e.g. as a class member) or you wish to pass it across threads,
 * use a JavaGlobalRef instead.
 */
template<typename T>
class JavaLocalRef : public JavaRef<T> {
public:
  JavaLocalRef() = default;

  JavaLocalRef(std::nullptr_t) {} // NOLINT(runtime/explicit)

  JavaLocalRef(JNIEnv* env, const JavaRef<T>& other)
      : env_(env) {
    Reset(other.obj(), OwnershipPolicy::RETAIN);
  }

  //!< Allow constructing e.g. JavaLocalRef<jobject> from
  //!< JavaLocalRef<jstring>.
  template<typename G>
  JavaLocalRef(JavaLocalRef<G>&& other)
      : env_(other.env()) {
    Reset(other.Release(), OwnershipPolicy::ADOPT);
  }

  JavaLocalRef(const JavaLocalRef& other)
      : env_(other.env_) {
    Reset(other.obj(), OwnershipPolicy::RETAIN);
  }

  //!< Assumes that |obj| is a reference to a Java object and takes
  //!< ownership  of this  reference. This should preferably not be used
  //!< outside of JNI helper functions.
  JavaLocalRef(JNIEnv* env, T obj)
      : JavaRef<T>(obj),
        env_(env) {}

  ~JavaLocalRef() {
    if (obj_ != nullptr) {
      env_->DeleteLocalRef(obj_);
    }
  }

  void operator=(const JavaLocalRef& other) {
    Reset(other.obj(), OwnershipPolicy::RETAIN);
  }

  void operator=(JavaLocalRef&& other) {
    Reset(other.Release(), OwnershipPolicy::ADOPT);
  }

  //!< Releases the reference to the caller. The caller *must* delete the
  //!< reference when it is done with it. Note that calling a Java method
  //!< is *not* a transfer of ownership and Release() should not be used.
  T Release() {
    T obj = static_cast<T>(obj_);
    obj_ = nullptr;
    return obj;
  }

  [[nodiscard]] JNIEnv* env() const { return env_; }

private:
  using JavaRef<T>::obj_;

  enum OwnershipPolicy {
    //!< The scoped object takes ownership of an object by taking over an
    //!< existing
    //!< ownership claim.
    ADOPT,
    //!< The scoped object will retain the the object and any initial ownership
    //!< is
    //!< not changed.
    RETAIN
  };

  void Reset(T obj, OwnershipPolicy policy) {
    if (obj_ != nullptr) {
      env_->DeleteLocalRef(obj_);
    }
    obj_ = (obj != nullptr && policy == OwnershipPolicy::RETAIN)
           ? env_->NewLocalRef(obj)
           : obj;
  }

  JNIEnv* const env_ = JvmEnv::get().getEnvironment();
};

template<typename T>
class JavaGlobalRef : public JavaRef<T> {
public:
  using JavaRef<T>::obj_;
  JavaGlobalRef() = default;

  explicit constexpr JavaGlobalRef(std::nullptr_t) {}

  explicit JavaGlobalRef(JNIEnv* env, const JavaRef<T>& other) noexcept
      : JavaRef<T>(static_cast<T>(env->NewGlobalRef(other.obj()))) {}

  explicit JavaGlobalRef(const JavaLocalRef<T>& other) noexcept
      : JavaGlobalRef(other.env(), other) {}

  JavaGlobalRef(JavaGlobalRef&& other) noexcept
      : JavaRef<T>(other.Release()) {}

  ~JavaGlobalRef() {
    if (obj_ != nullptr) {
      JvmEnv::get().getEnvironment()->DeleteGlobalRef(obj_);
      obj_ = nullptr;
    }
  }

  JavaGlobalRef(const JavaGlobalRef&) = delete;
  JavaGlobalRef& operator=(const JavaGlobalRef&) = delete;

  void operator=(const JavaRef<T>& other) {
    JNIEnv* env = JvmEnv::get().getEnvironment();
    if (obj_ != nullptr) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = other.is_null() ? nullptr : env->NewGlobalRef(other.obj());
  }

  void operator=(std::nullptr_t) {
    if (obj_ != nullptr) {
      JvmEnv::get().getEnvironment()->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }

  //!< Releases the reference to the caller. The caller *must* delete the
  //!< reference when it is done with it. Note that calling a Java method
  //!< is *not* a transfer of ownership and Release() should not be used.
  T Release() {
    T obj = static_cast<T>(obj_);
    obj_ = nullptr;
    return obj;
  }
};

template<typename T>
inline JavaLocalRef<T> static_java_ref_cast(JNIEnv* env, JavaRef<jobject> const& ref) {
  JavaLocalRef<jobject> owned_ref(env, ref);
  return JavaLocalRef<T>(env, static_cast<T>(owned_ref.Release()));
}

} // namespace jni

template<typename T>
using ScopedJavaLocalRef = jni::JavaLocalRef<T>;

template<typename T>
using ScopedJavaGlobalRef = jni::JavaGlobalRef<T>;

using ScopedLocalObject    = jni::JavaLocalRef<jobject>;
using ScopedLocalClass    = jni::JavaLocalRef<jclass>;
using ScopedGlobalClass = jni::JavaGlobalRef<jclass>;
using ScopedLocalByteArray = jni::JavaLocalRef<jbyteArray>;

#endif // CORAL_JNI_SCOPED_JAVA_REF_H
