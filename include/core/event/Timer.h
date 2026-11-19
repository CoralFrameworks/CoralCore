/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_EVENT_TIMER_H
#define CORAL_EVENT_TIMER_H

#include <core/event/Definitions.h>

#include <atomic>
#include <functional>
#include <memory>

namespace coral {

class EventLooper;

/**
 * A one-shot or repeating callback on a looper's thread.
 *
 * Built on `EventHandler`'s delayed messages, so it needs a running `EventThread`/looper and shares
 * its delivery rules: a timer message due during a sync barrier is held like any other synchronous
 * message, and a quit drops messages that are not yet due.
 *
 * The callback runs on the looper thread. `start`, `stop` and `setInterval` may be called from any
 * thread; a callback must not capture a `shared_ptr` to its own Timer unless it is also prepared for
 * the resulting cycle.
 */
class Timer final : public std::enable_shared_from_this<Timer> {
public:
  using Callback = std::function<void()>;

  /**
   * Creates a timer on @p looper. It is stopped until `start()` is called.
   *
   * @param interval  the delay between fires, or the first delay for a one-shot timer.
   * @param repeating when true the callback is rescheduled after every fire.
   * @return null when @p looper or @p callback is empty.
   */
  [[nodiscard]] static std::shared_ptr<Timer> create(const std::shared_ptr<EventLooper>& looper,
    TimeDelta interval, Callback callback, bool repeating = false);

  ~Timer();

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  /** Starts with the configured interval. Replaces any pending fire. */
  void start();

  /** Starts one fire after @p delay, keeping the configured interval for a repeated timer. */
  void start(TimeDelta delay);

  /** Cancels the pending fire; a callback already running is not interrupted. */
  void stop();

  [[nodiscard]] bool running() const noexcept { return mRunning.load(std::memory_order_acquire); }
  [[nodiscard]] TimeDelta interval() const noexcept {
    return TimeDelta(mIntervalNanos.load(std::memory_order_acquire));
  }
  void setInterval(TimeDelta interval);

private:
  class Handler;

  Timer(TimeDelta interval, Callback callback, bool repeating);
  void arm(TimeDelta delay);
  void fire();

  std::shared_ptr<Handler> mHandler;
  std::atomic<int64_t> mIntervalNanos{ 0 };
  Callback mCallback;
  bool mRepeating = false;
  std::atomic<bool> mRunning{ false };
};

} // namespace coral

#endif // CORAL_EVENT_TIMER_H
