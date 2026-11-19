/*
 * Copyright (C) 2023 The Coral Project
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

#ifndef CORAL_TASK_ATOMIC_WAIT_H
#define CORAL_TASK_ATOMIC_WAIT_H

#include <atomic>
#include <cstdint>
#include <limits>

// =============================================================================
//  Platform detection
//
//  This header is included from other public headers, so the platform SDK is
//  pulled in only for the three families that have a kernel wait primitive.
// =============================================================================

#if defined(_WIN32)
#define CORAL_ATOMIC_WAIT_WINDOWS 1
#elif defined(__APPLE__)
#define CORAL_ATOMIC_WAIT_DARWIN 1
#elif defined(__linux__) || defined(__ANDROID__)
#define CORAL_ATOMIC_WAIT_LINUX 1
#else
#define CORAL_ATOMIC_WAIT_FALLBACK 1
#endif

#if defined(CORAL_ATOMIC_WAIT_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(CORAL_ATOMIC_WAIT_LINUX)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <ctime>
#elif defined(CORAL_ATOMIC_WAIT_DARWIN)
#include <cerrno>
#include <cstddef>
#else
#include <thread>
#endif

namespace coral {

/**
 * A blocking wait on an atomic word, built on the kernel's wait/wake primitive
 * instead of a mutex.
 *
 * A thread cannot be put to sleep without asking the kernel, so "lock-free" here
 * means: no mutex is taken, no critical section exists, and the fast path is a
 * single atomic read or compare-exchange. The kernel call happens only when a
 * thread actually has to sleep. Each platform already has the primitive:
 *
 *  - Windows: `WaitOnAddress` / `WakeByAddress*`, which is the futex interface;
 *  - Apple: `__ulock_wait` / `__ulock_wake` with `UL_COMPARE_AND_WAIT`;
 *  - Linux and Android: `FUTEX_WAIT_PRIVATE` / `FUTEX_WAKE_PRIVATE`.
 *
 * Every wait also re-reads the word before sleeping and after waking, so a lost
 * wake-up can cost latency but never correctness.
 */
class AtomicWait {
public:
  /** A timeout that means "block until notified". */
  static constexpr int64_t kWaitForever = -1;

  AtomicWait() = delete;

  /**
   * Blocks while `word` still holds `expected`, or until the timeout expires.
   *
   * @param word         the word to watch.
   * @param expected     the value `word` must still hold in order to sleep. If it
   *                     has already moved on, this returns immediately, which is
   *                     what makes a wake-up that races with the sleep harmless.
   * @param timeoutNanos how long to sleep at most, or `kWaitForever`.
   * @return `true` when `word` may have changed (including a spurious wake-up),
   *         `false` when the timeout expired.
   *
   * @discussion
   * Either answer means "re-read your condition". `false` does not mean the wait
   * failed, and `true` does not mean the state actually changed.
   */
  [[nodiscard]] static bool wait(const std::atomic<uint32_t>& word,
      uint32_t expected,
      int64_t timeoutNanos) noexcept;

  /** Wakes one thread waiting on `word`, if there is one. */
  static void notifyOne(const std::atomic<uint32_t>& word) noexcept;

  /** Wakes every thread waiting on `word`. */
  static void notifyAll(const std::atomic<uint32_t>& word) noexcept;

  /**
   * Whether this build has a real kernel wait.
   *
   * A build without one still works — the fallback yields instead of sleeping —
   * so callers should treat this as a performance detail, not a capability.
   */
  [[nodiscard]] static constexpr bool supported() noexcept {
#if defined(CORAL_ATOMIC_WAIT_FALLBACK)
    return false;
#else
    return true;
#endif
  }
};

/**
 * A lock-free "state changed" counter with a blocking wait.
 *
 * This is what a queue needs in place of a condition variable: publishing work
 * bumps the sequence, and a consumer sleeps on the sequence rather than on a
 * mutex. Bumping and reading are each a single atomic operation, so neither side
 * takes a lock.
 *
 * The counter is 32-bit and allowed to wrap; only equality against a previously
 * observed value matters, so wrapping is harmless.
 */
class EventCounter {
public:
  EventCounter() noexcept = default;

  EventCounter(const EventCounter&) = delete;
  EventCounter& operator=(const EventCounter&) = delete;

  /** Publishes a change and wakes one waiter. */
  void signal() noexcept {
    sequence_.fetch_add(1, std::memory_order_release);
    AtomicWait::notifyOne(sequence_);
  }

  /** Publishes a change and wakes every waiter. */
  void signalAll() noexcept {
    sequence_.fetch_add(1, std::memory_order_release);
    AtomicWait::notifyAll(sequence_);
  }

  /** The current sequence, to hand back to `waitForChange()`. */
  [[nodiscard]] uint32_t sequence() const noexcept {
    return sequence_.load(std::memory_order_acquire);
  }

  /**
   * Blocks until `sequence()` differs from `observed`, or the timeout expires.
   *
   * @param observed     a value read *before* the guarded state was inspected.
   * @param timeoutNanos how long to wait at most, or `kWaitForever`.
   * @return `true` when the sequence may have changed, `false` on timeout.
   *
   * @discussion
   * `observed` must be read before checking the state being waited on. Reading
   * it afterwards would miss a change that landed in between and sleep straight
   * through the very event being waited for.
   */
  [[nodiscard]] bool waitForChange(const uint32_t observed, const int64_t timeoutNanos) const noexcept {
    if (sequence_.load(std::memory_order_acquire) != observed) {
      return true;
    }
    return AtomicWait::wait(sequence_, observed, timeoutNanos);
  }

private:
  alignas(4) mutable std::atomic<uint32_t> sequence_{ 0 };
};

// =============================================================================
//  Implementation
// =============================================================================

#if defined(CORAL_ATOMIC_WAIT_WINDOWS)

inline bool AtomicWait::wait(const std::atomic<uint32_t>& word,
    const uint32_t expected,
    const int64_t timeoutNanos) noexcept {
  // WaitOnAddress takes a relative timeout in milliseconds and re-reads the word
  // itself, so it already has the futex contract.
  DWORD millis = INFINITE;
  if (timeoutNanos >= 0) {
    const int64_t rounded = (timeoutNanos + 999999) / 1000000;
    millis = rounded >= static_cast<int64_t>(INFINITE) - 1 ? INFINITE - 1 : static_cast<DWORD>(rounded);
  }
  auto* address = const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(&word));
  // WaitOnAddress only reads the comparison value, so handing it a mutable copy
  // avoids casting the const away from the caller's word.
  uint32_t compare = expected;
  return ::WaitOnAddress(address, &compare, sizeof(uint32_t), millis) != FALSE;
}

inline void AtomicWait::notifyOne(const std::atomic<uint32_t>& word) noexcept {
  ::WakeByAddressSingle(const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(&word)));
}

inline void AtomicWait::notifyAll(const std::atomic<uint32_t>& word) noexcept {
  ::WakeByAddressAll(const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(&word)));
}

#elif defined(CORAL_ATOMIC_WAIT_LINUX)

namespace detail {

/** The raw futex syscall, returning 0 on success and -1 with `errno` set. */
inline int futexSyscall(std::atomic<uint32_t>& word,
    const int operation,
    const uint32_t value,
    struct timespec* timeout) noexcept {
#if defined(__NR_futex)
  return static_cast<int>(::syscall(__NR_futex, reinterpret_cast<uint32_t*>(&word), operation, value,
      timeout, nullptr, 0));
#else
  return static_cast<int>(::syscall(__NR_futex_time64, reinterpret_cast<uint32_t*>(&word), operation,
      value, timeout, nullptr, 0));
#endif
}

/**
 * A relative timeout in nanoseconds as the timespec `FUTEX_WAIT` expects.
 *
 * @discussion `FUTEX_WAIT` reads its timeout as a duration, unlike `FUTEX_WAIT_BITSET`
 * which reads an absolute deadline. Handing it an absolute `CLOCK_MONOTONIC` value makes
 * the kernel sleep for "time since boot plus the timeout" instead of the timeout, so a
 * delayed wake-up never happens and only an explicit `FUTEX_WAKE` ends the wait. That is
 * why the duration is converted here rather than the clock being read.
 */
[[nodiscard]] inline struct timespec relativeTimeout(const int64_t timeoutNanos) noexcept {
  constexpr int64_t kNanosPerSecond = 1000000000LL;
  struct timespec ts{};
  if (timeoutNanos > 0) {
    ts.tv_sec = static_cast<time_t>(timeoutNanos / kNanosPerSecond);
    ts.tv_nsec = static_cast<long>(timeoutNanos % kNanosPerSecond);
  }
  return ts;
}

} // namespace detail

inline bool AtomicWait::wait(const std::atomic<uint32_t>& word,
    const uint32_t expected,
    const int64_t timeoutNanos) noexcept {
  auto& mutableWord = const_cast<std::atomic<uint32_t>&>(word);
  if (timeoutNanos < 0) {
    if (detail::futexSyscall(mutableWord, FUTEX_WAIT_PRIVATE, expected, nullptr) == 0) {
      return true;
    }
    // EAGAIN: the value changed before the sleep started, which is the state the
    // caller wanted to learn about. EINTR: a signal, so re-check.
    return errno == EAGAIN || errno == EINTR;
  }

  struct timespec timeout = detail::relativeTimeout(timeoutNanos);
  if (detail::futexSyscall(mutableWord, FUTEX_WAIT_PRIVATE, expected, &timeout) == 0) {
    return true;
  }
  return errno != ETIMEDOUT;
}

inline void AtomicWait::notifyOne(const std::atomic<uint32_t>& word) noexcept {
  detail::futexSyscall(const_cast<std::atomic<uint32_t>&>(word), FUTEX_WAKE_PRIVATE, 1, nullptr);
}

inline void AtomicWait::notifyAll(const std::atomic<uint32_t>& word) noexcept {
  detail::futexSyscall(const_cast<std::atomic<uint32_t>&>(word), FUTEX_WAKE_PRIVATE,
      std::numeric_limits<int>::max(), nullptr);
}

#elif defined(CORAL_ATOMIC_WAIT_DARWIN)

namespace detail {

// From <sys/ulock.h>. Declared locally so this header does not depend on a
// private SDK header.
constexpr uint32_t kUlockCompareAndWait = 1;
constexpr uint32_t kUlockWake = 2;
/** ULF_WAKE_ONE: wake a single waiter instead of all of them. */
constexpr uint32_t kUlockWakeOne = 0x00000001;

extern "C" int __ulock_wait(uint32_t operation, void* address, uint64_t value, uint32_t timeout);
extern "C" int __ulock_wake(uint32_t operation, void* address, uint64_t wakeValue);

/** `__ulock_wait` takes microseconds and treats 0 as "no timeout". */
[[nodiscard]] inline uint32_t microsFromNanos(const int64_t timeoutNanos) noexcept {
  if (timeoutNanos <= 0) {
    return 0;
  }
  const uint64_t micros = static_cast<uint64_t>((timeoutNanos + 999) / 1000);
  return micros >= std::numeric_limits<uint32_t>::max() ? std::numeric_limits<uint32_t>::max()
                                                        : static_cast<uint32_t>(micros);
}

} // namespace detail

inline bool AtomicWait::wait(const std::atomic<uint32_t>& word,
    const uint32_t expected,
    const int64_t timeoutNanos) noexcept {
  auto& mutableWord = const_cast<std::atomic<uint32_t>&>(word);
  const int result = detail::__ulock_wait(detail::kUlockCompareAndWait, &mutableWord, expected,
      timeoutNanos < 0 ? 0 : detail::microsFromNanos(timeoutNanos));
  if (result == 0) {
    return true;
  }
  // ETIMEDOUT is the only answer that means "nothing happened".
  return -result != ETIMEDOUT;
}

inline void AtomicWait::notifyOne(const std::atomic<uint32_t>& word) noexcept {
  detail::__ulock_wake(detail::kUlockWake | detail::kUlockWakeOne,
      const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(&word)), 0);
}

inline void AtomicWait::notifyAll(const std::atomic<uint32_t>& word) noexcept {
  detail::__ulock_wake(detail::kUlockWake, const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(&word)), 0);
}

#else // CORAL_ATOMIC_WAIT_FALLBACK

inline bool AtomicWait::wait(const std::atomic<uint32_t>& word,
    const uint32_t,
    const int64_t timeoutNanos) noexcept {
  // Without a kernel primitive there is nothing to sleep on, so yield instead of
  // spinning hard. Progress is preserved; the CPU cost is the trade-off, and the
  // caller still re-checks its condition afterwards.
  (void) word;
  (void) timeoutNanos;
  std::this_thread::yield();
  return true;
}

inline void AtomicWait::notifyOne(const std::atomic<uint32_t>&) noexcept {
}

inline void AtomicWait::notifyAll(const std::atomic<uint32_t>&) noexcept {
}

#endif

} // namespace coral

#endif // CORAL_TASK_ATOMIC_WAIT_H
