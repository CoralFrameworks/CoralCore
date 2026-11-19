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

#include <core/event/Timer.h>

#include <core/event/EventHandler.h>
#include <core/event/EventLooper.h>
#include <core/event/Message.h>

namespace coral {

namespace {
constexpr int kTimerMessage = 0x54494d45; // 'TIME'
}

class Timer::Handler final : public EventHandler {
public:
  Handler(const std::shared_ptr<EventLooper>& looper, Timer& owner)
      : EventHandler(looper, nullptr), mOwner(owner) {}

  void handleMessage(Message* message) override {
    if (message != nullptr && message->what == kTimerMessage) mOwner.fire();
  }

private:
  Timer& mOwner;
};

std::shared_ptr<Timer> Timer::create(const std::shared_ptr<EventLooper>& looper, const TimeDelta interval,
    Callback callback, const bool repeating) {
  if (looper == nullptr || !callback) return nullptr;
  auto timer = std::shared_ptr<Timer>(new Timer(interval, std::move(callback), repeating));
  timer->mHandler = std::make_shared<Handler>(looper, *timer);
  return timer;
}

Timer::Timer(const TimeDelta interval, Callback callback, const bool repeating)
    : mIntervalNanos(interval.count()), mCallback(std::move(callback)), mRepeating(repeating) {}

Timer::~Timer() {
  stop();
  if (mHandler != nullptr) mHandler->removeMessages(kTimerMessage);
}

void Timer::start() {
  start(interval());
}

void Timer::start(const TimeDelta delay) {
  mRunning.store(true, std::memory_order_release);
  arm(delay);
}

void Timer::stop() {
  mRunning.store(false, std::memory_order_release);
  if (mHandler != nullptr) mHandler->removeMessages(kTimerMessage);
}

void Timer::setInterval(const TimeDelta interval) {
  mIntervalNanos.store(interval.count(), std::memory_order_release);
  if (running()) arm(interval);
}

void Timer::arm(const TimeDelta delay) {
  if (mHandler == nullptr || !running()) return;
  mHandler->removeMessages(kTimerMessage);
  mHandler->sendEmptyMessageDelayed(kTimerMessage, delay);
}

void Timer::fire() {
  if (!running()) return;
  mCallback();
  // The callback may have stopped or restarted the timer; only a timer that is still running is
  // rescheduled, and its interval may have changed in the meantime.
  if (mRepeating && running()) arm(interval());
}

} // namespace coral
