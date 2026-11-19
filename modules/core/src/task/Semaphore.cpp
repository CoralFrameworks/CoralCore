/*
 * Copyright (C) 2025 The Coral Project
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

#include <core/task/Semaphore.h>

#include <utils/Assert.h>
#include <utils/compiler.h>

#include "eintr_wrapper.h"

#if defined(CORAL_BUILD_FOR_MAC) || defined(CORAL_BUILD_FOR_IOS)
#include <dispatch/dispatch.h>

namespace coral {

class PlatformSemaphore {
public:
  explicit PlatformSemaphore(uint32_t count)
      : sem_(dispatch_semaphore_create(count)), initial_(count) {}

  ~PlatformSemaphore() {
    for (uint32_t i = 0; i < initial_; ++i) {
      Signal();
    }
    if (sem_ != nullptr) {
      dispatch_release(reinterpret_cast<dispatch_object_t>(sem_));
      sem_ = nullptr;
    }
  }

  bool isValid() const { return sem_ != nullptr; }

  bool wait() {
    if (sem_ == nullptr) {
      return false;
    }
    return dispatch_semaphore_wait(sem_, DISPATCH_TIME_FOREVER) == 0;
  }

  bool tryWait() {
    if (sem_ == nullptr) {
      return false;
    }

    return dispatch_semaphore_wait(sem_, DISPATCH_TIME_NOW) == 0;
  }

  void signal() {
    if (sem_ != nullptr) {
      dispatch_semaphore_signal(sem_);
    }
  }

private:
  dispatch_semaphore_t sem_;
  const uint32_t initial_;
};

} // namespace coral

#elif defined(CORAL_BUILD_FOR_WIN)

#include <Windows.h>

namespace coral {

class PlatformSemaphore {
public:
  explicit PlatformSemaphore(const uint32_t count) noexcept
      : mSemaphore(CreateSemaphore(nullptr, static_cast<long>(count), LONG_MAX, nullptr)) {}

  ~PlatformSemaphore() {
    if (mSemaphore != nullptr) {
      CloseHandle(mSemaphore);
      mSemaphore = nullptr;
    }
  }

  [[nodiscard]] bool isValid() const { return mSemaphore != nullptr; }

  [[nodiscard]] bool wait() const {
    if (mSemaphore == nullptr) {
      return false;
    }

    return WaitForSingleObject(mSemaphore, INFINITE) == WAIT_OBJECT_0;
  }

  [[nodiscard]] bool tryWait() const {
    if (mSemaphore == nullptr) {
      return false;
    }

    return WaitForSingleObject(mSemaphore, 0) == WAIT_OBJECT_0;
  }

  void signal() const {
    if (mSemaphore != nullptr) {
      ReleaseSemaphore(mSemaphore, 1, nullptr);
    }
  }

private:
  HANDLE mSemaphore;
};

} // namespace coral

#else

#include <semaphore.h>

namespace coral {

class PlatformSemaphore {
public:
  explicit PlatformSemaphore(uint32_t count) noexcept
      : valid_(::sem_init(&sem_, 0 /* not shared */, count) == 0) {}

  ~PlatformSemaphore() {
    if (valid_) {
      int result = ::sem_destroy(&sem_);
      (void)result;
      // Can only be EINVAL which should not be possible since we checked for validity.
      CORAL_ASSERT(result == 0);
    }
  }

  [[nodiscard]] bool isValid() const { return valid_; }

  bool wait() {
    if (!valid_) {
      return false;
    }

    return CORAL_HANDLE_EINTR(::sem_wait(&sem_)) == 0;
  }

  bool tryWait() {
    if (!valid_) {
      return false;
    }

    return CORAL_HANDLE_EINTR(::sem_trywait(&sem_)) == 0;
  }

  void signal() {
    if (!valid_) {
      return;
    }
    ::sem_post(&sem_);
    return;
  }

private:
  bool valid_;
  sem_t sem_;
};

} // namespace coral

#endif


namespace coral {

Semaphore::Semaphore(uint32_t count) noexcept: impl_(new PlatformSemaphore(count)) {}

Semaphore::~Semaphore() = default;

bool Semaphore::isValid() const {
  return impl_->isValid();
}

bool Semaphore::wait() const {
  return impl_->wait();
}

bool Semaphore::tryWait() const {
  return impl_->tryWait();
}

void Semaphore::signal() const {
  return impl_->signal();
}

} // namespace coral
