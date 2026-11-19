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

#ifndef CORAL_EVENT_EVENT_THREAD_H
#define CORAL_EVENT_EVENT_THREAD_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace coral {

class EventHandler;
class EventLooper;

/**
 * A thread that has a {@link EventLooper}.
 *
 * The {@link EventLooper} can then be used to create {@link EventHandler}s.
 * {@link EventThread::start()} must be called before the thread runs.
 *
 * @discussion
 * The lifecycle is deliberately strict, because getting it wrong is a crash
 * rather than a glitch:
 *
 *  - `start()` returns only once the looper exists, so `getLooper()` right after
 *    it never returns null for a started thread;
 *  - the destructor quits the thread. A `std::thread` that is still joinable when
 *    it is destroyed calls `std::terminate`, so a thread left running would take
 *    the process down;
 *  - `quit()` joins from another thread but detaches when called from the looper
 *    thread itself, where joining would deadlock;
 *  - `isAlive()`, `getLooper()` and `start()` are safe to call from any thread.
 *    The looper and the thread id are published with release/acquire, so no
 *    reader ever sees a half-written value.
 */
class EventThread final {
public:
  explicit EventThread(std::string name) noexcept;

  /**
   * Stops the thread if it is still running, then releases it.
   */
  ~EventThread();

  EventThread(const EventThread&) = delete;
  EventThread& operator=(const EventThread&) = delete;

  EventThread(EventThread&&) = delete;
  EventThread& operator=(EventThread&&) = delete;

  /**
   * Starts the thread and returns once its looper is ready.
   *
   * Calling this on an already started thread does nothing.
   */
  void start();

  /**
   * Quits the looper, dropping work that is not due yet.
   *
   * @return `false` when the thread was not running.
   */
  bool quit();

  /**
   * Quits the looper once the work that is already due has been delivered.
   *
   * @return `false` when the thread was not running.
   */
  bool quitSafety();

  /** The thread body. Runs the looper until it is asked to quit. */
  void run();

  /** Whether the looper thread is currently running. */
  [[nodiscard]] bool isAlive() const;

  /**
   * The looper of this thread, or null when the thread is not running.
   *
   * Safe to call from any thread, including before `start()`.
   */
  const std::shared_ptr<EventLooper>& getLooper();

  class EventLatch {
  public:
    /**
     * Creates a event latch with a specified count. The minimum useful value is 1.
     * @param count the latch counter initial value
     */
    explicit EventLatch(size_t count) noexcept;

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
     * Resets the event latch to the given value.
     *
     * @param new_count New latch count. A value of zero will immediately unblock all waiting
     * threads.
     *
     * @warning Use with caution. It's only safe to reset the latch count when you're sure
     * that no threads are waiting in await(). This can be guaranteed in various ways, for
     * instance, if you have a single thread calling await(), you could call reset() from that
     * thread, or you could use a CyclicBarrier to make sure all threads using the EventLatch
     * are at a known place (i.e.: not in await()) when reset() is called.
     */
    void reset(size_t new_count) noexcept;

    /**
     * @return the number of times latch() has been called since construction or reset.
     * @see reset(),  EventLatch(size_t)
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

private:
  /** Releases the thread handle, joining or detaching as asked. */
  void releaseThread(bool join);

  /** The name given to the looper thread. */
  const std::string threadName;

  /**
   * Guards the thread handle and the start/stop handshake.
   *
   * This is the only mutex in the event implementation, and it is not on any
   * message path: it exists because `std::thread` itself cannot be manipulated
   * from several threads at once.
   */
  mutable std::mutex mStartLock;

  /** Fired once `run()` has published the looper. */
  mutable EventLatch barrier;

  /** The looper thread's own id, published by `run()`. */
  std::atomic<std::thread::id> mThreadId{};

  /** The looper, published by `run()` once it is fully built. */
  std::shared_ptr<EventLooper> mLooper;

  /** The thread handle, owned under `mStartLock`. */
  std::unique_ptr<std::thread> mThread;
};

} // namespace coral

#endif // CORAL_EVENT_EVENT_THREAD_H
