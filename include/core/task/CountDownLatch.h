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

#ifndef CORAL_TASK_COUNT_DOWN_LATCH_H
#define CORAL_TASK_COUNT_DOWN_LATCH_H

#include <core/task/AtomicWait.h>
#include <core/task/TaskBase.h>

#include <atomic>
#include <cstdint>

namespace coral {

/**
 * A count down latch is used to block one or several threads until the latch is signaled
 * a certain number of times.
 *
 * Threads entering the latch are blocked until the latch is signaled enough times.
 *
 * @discussion
 * This is lock-free: the counter is an atomic, signalers wake waiters through the
 * platform's wait primitive, and nothing here can block on a mutex. That also
 * makes the `noexcept` on every method honest — `std::mutex::lock()` can throw
 * `std::system_error`, which a `noexcept` method may not do without terminating
 * the process.
 */
class CountDownLatch : public TaskBase {
public:
  /**
   * Creates a count down latch with a specified count. The minimum useful value is 1.
   * @param count the latch counter initial value
   */
  explicit CountDownLatch(size_t count) noexcept;

  /**
   * Blocks until latch() is called \p count times.
   * @see CountDownLatch(size_t)
   */
  void await() noexcept;

  /**
   * Releases threads blocked in await() when called \p count times. Calling latch() more than
   * \p count times has no effect.
   * @see reset()
   */
  void latch() noexcept;

  /**
   * Resets the count-down latch to the given value.
   *
   * @param new_count New latch count. A value of zero will immediately unblock all waiting
   * threads.
   *
   * @warning Use with caution. It's only safe to reset the latch count when you're sure
   * that no threads are waiting in await(). This can be guaranteed in various ways, for
   * instance, if you have a single thread calling await(), you could call reset() from that
   * thread, or you could use a CyclicBarrier to make sure all threads using the CountDownLatch
   * are at a known place (i.e.: not in await()) when reset() is called.
   */
  void reset(size_t new_count) noexcept;

  /**
   * @return the number of times latch() has been called since construction or reset.
   * @see reset(),  CountDownLatch(size_t)
   */
  [[nodiscard]] size_t getCount() const noexcept;

private:
  const uint32_t m_initial_count;

  /**
   * The remaining count, and the word waiters sleep on.
   *
   * A waiter hands the kernel the value it saw, so the kernel wakes it only if
   * the count has actually moved. The count is clamped to 32 bits because the
   * wait primitive compares a 32-bit word.
   */
  alignas(4) std::atomic<uint32_t> m_remaining_count;
};

} // namespace coral

#endif //CORAL_TASK_COUNT_DOWN_LATCH_H
