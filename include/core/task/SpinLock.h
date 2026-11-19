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

#ifndef CORAL_PRIVATE_BASE_SPIN_LOCK_H
#define CORAL_PRIVATE_BASE_SPIN_LOCK_H

#include <atomic>

namespace coral {

class SpinLock {
public:
  constexpr SpinLock() = default;

  void acquire() {
    // To act as a mutex, we need an acquire barrier when we acquire the lock.
    if (locked.exchange(true, std::memory_order_acquire)) {
      // Lock was contended.  Fall back to an out-of-line spin loop.
      this->contendedAcquire();
    }
  }

  /**
   * Acquire the lock or fail (quickly). Lets the caller decide to do something other than wait.
   */
  bool tryAcquire() {
    // To act as a mutex, we need an acquire barrier when we acquire the lock.
    if (locked.exchange(true, std::memory_order_acquire)) {
      // Lock was contended. Let the caller decide what to do.
      return false;
    }
    return true;
  }

  void release() {
    // To act as a mutex, we need a release barrier when we release the lock.
    locked.store(false, std::memory_order_release);
  }

private:
  void contendedAcquire();

  std::atomic<bool> locked{ false };
};

class AutoSpinLock {
public:
  explicit AutoSpinLock(SpinLock& mutex) noexcept
      : spinLock(mutex) {
    spinLock.acquire();
  }

  ~AutoSpinLock() { spinLock.release(); }

private:
  SpinLock& spinLock;
};

} // namespace coral

#endif //CORAL_PRIVATE_BASE_SPIN_LOCK_H
