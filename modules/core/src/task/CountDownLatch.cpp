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

#include <core/task/CountDownLatch.h>

#include <algorithm>
#include <limits>

namespace coral {

namespace {

/** The largest count a 32-bit wait word can express. */
constexpr size_t kMaxCount = std::numeric_limits<uint32_t>::max();

/** Clamps a requested count into the range the wait word supports. */
uint32_t clampCount(const size_t count) noexcept {
  return static_cast<uint32_t>(std::min(count, kMaxCount));
}

} // namespace anonymous

CountDownLatch::CountDownLatch(const size_t count) noexcept
    : m_initial_count(clampCount(count)),
      m_remaining_count(clampCount(count)) {
}

void CountDownLatch::reset(const size_t count) noexcept {
  const uint32_t clamped = clampCount(count);
  m_remaining_count.store(clamped, std::memory_order_release);
  // Every waiter has to re-read the counter, so wake them all. A waiter whose
  // count is still non-zero simply sleeps again.
  AtomicWait::notifyAll(m_remaining_count);
}

void CountDownLatch::await() noexcept {
  for (;;) {
    const uint32_t remaining = m_remaining_count.load(std::memory_order_acquire);
    if (remaining == 0) {
      return;
    }
    // Sleep on the counter itself: the kernel only sleeps while it still holds
    // `remaining`, so a latch that lands before the sleep is not missed.
    (void) AtomicWait::wait(m_remaining_count, remaining, AtomicWait::kWaitForever);
  }
}

void CountDownLatch::latch() noexcept {
  uint32_t remaining = m_remaining_count.load(std::memory_order_acquire);
  while (remaining > 0) {
    // A compare-exchange rather than a fetch_sub, so the counter can never wrap
    // past zero when latch() is called more often than the initial count.
    if (m_remaining_count.compare_exchange_weak(remaining, remaining - 1, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      if (remaining - 1 == 0) {
        // Reaching zero is the only transition waiters care about.
        AtomicWait::notifyAll(m_remaining_count);
      }
      return;
    }
  }
}

size_t CountDownLatch::getCount() const noexcept {
  const uint32_t remaining = m_remaining_count.load(std::memory_order_acquire);
  return static_cast<size_t>(m_initial_count - remaining);
}

} // namespace coral
