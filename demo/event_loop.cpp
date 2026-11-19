/*
 * Copyright (C) 2026 The Coral Project
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

// =============================================================
//  event_loop.cpp
//
//  The `event` module: an EventThread owns a looper, an
//  EventHandler posts closures and messages to it, and an
//  event::Timer repeats a callback on the same thread. Every
//  wait below is bounded, so a regression fails the demo instead
//  of hanging it.
//
//    coralcore_demo_event_loop
// =============================================================

#include <core/event/EventHandler.h>
#include <core/event/EventLooper.h>
#include <core/event/EventThread.h>
#include <core/event/Message.h>
#include <core/event/Timer.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

namespace {

int gChecks = 0;
int gFailures = 0;

void check(const bool condition, const char* what) {
  ++gChecks;
  std::printf("  %s %s\n", condition ? "ok  " : "FAIL", what);
  if (!condition) {
    ++gFailures;
  }
}

/** Spins until `predicate` holds or the deadline passes. */
template<typename Predicate>
bool waitFor(Predicate&& predicate, const std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return predicate();
}

/**
 * A handler subclass, the usual way to receive Message objects: the looper calls handleMessage() on
 * its own thread. The counters are atomic because main() reads them while the looper writes them.
 */
class PingHandler final : public coral::EventHandler {
public:
  explicit PingHandler(const std::shared_ptr<coral::EventLooper>& looper)
    : EventHandler(looper, nullptr) {}

  void handleMessage(coral::Message* message) override {
    if (message != nullptr && message->what == kPing) {
      lastValue.store(message->arg1, std::memory_order_relaxed);
      pings.fetch_add(1, std::memory_order_relaxed);
    }
  }

  static constexpr int kPing = 1;

  std::atomic<int> pings{ 0 };
  std::atomic<int> lastValue{ 0 };
};

} // namespace

int main() {
  std::printf("CoralCore demo: event_loop\n\n");

  const std::thread::id callerThread = std::this_thread::get_id();

  coral::EventThread thread("coralcore-demo");
  thread.start();
  check(thread.isAlive(), "EventThread::start() leaves the thread running");
  check(thread.getLooper() != nullptr, "the started thread published its EventLooper");
  check(!thread.getLooper()->isCurrentThread(), "the looper belongs to the event thread, not the caller");

  const std::shared_ptr<coral::EventLooper> looper = thread.getLooper();
  const std::shared_ptr<PingHandler> handler = std::make_shared<PingHandler>(looper);

  // A closure posted from this thread runs on the looper thread, where a looper is associated.
  std::atomic<int> closures{ 0 };
  std::atomic<bool> ranOnLooperThread{ false };
  const bool posted = handler->post([&closures, &ranOnLooperThread] {
    ranOnLooperThread.store(coral::EventLooper::hasLooper(), std::memory_order_relaxed);
    closures.fetch_add(1, std::memory_order_relaxed);
  });
  check(posted, "EventHandler::post() accepted a closure");

  // A Message goes through the same handler, with the code and both integer arguments intact.
  const bool sent = handler->sendMessage(coral::Message::obtain(handler, PingHandler::kPing, 42, 0));
  check(sent, "EventHandler::sendMessage() accepted a Message");

  // A delayed closure: it must not run before its deadline.
  std::atomic<int> delayed{ 0 };
  const bool delayedPosted = handler->postDelay([&delayed] { delayed.fetch_add(1, std::memory_order_relaxed); },
          std::chrono::milliseconds(30));
  check(delayedPosted, "EventHandler::postDelay() accepted a delayed closure");
  check(delayed.load() == 0, "the delayed closure did not run before its deadline");

  // A repeating timer on the same looper. Its callback stops the loop after a few fires.
  std::atomic<int> ticks{ 0 };
  const std::shared_ptr<coral::Timer> timer = coral::Timer::create(
          looper, std::chrono::milliseconds(5), [&ticks] { ticks.fetch_add(1, std::memory_order_relaxed); }, true);
  check(timer != nullptr, "Timer::create() returned a timer on a live looper");
  timer->start();

  const bool everythingRan = waitFor(
          [&] {
            return closures.load(std::memory_order_relaxed) == 1 &&
                   handler->pings.load(std::memory_order_relaxed) == 1 &&
                   delayed.load(std::memory_order_relaxed) == 1 && ticks.load(std::memory_order_relaxed) >= 3;
          },
          std::chrono::milliseconds(3000));

  check(everythingRan, "the closure, the Message, the delayed closure and the timer all ran");
  check(ranOnLooperThread.load(std::memory_order_relaxed), "the closure ran with a looper on its thread");
  check(handler->lastValue.load(std::memory_order_relaxed) == 42, "the Message carried its argument through");
  std::printf("  ticks: %d\n", ticks.load(std::memory_order_relaxed));

  // stop() cancels the pending fire; the count must not move afterwards.
  timer->stop();
  const int stoppedAt = ticks.load(std::memory_order_relaxed);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  check(ticks.load(std::memory_order_relaxed) == stoppedAt, "Timer::stop() cancelled the pending fire");

  // quitSafety() delivers work that is already due and drops work scheduled far in the future.
  const bool quitting = thread.quitSafety();
  check(quitting, "EventThread::quitSafety() reports a running thread");
  check(waitFor([&thread] { return !thread.isAlive(); }, std::chrono::milliseconds(3000)),
          "the event thread stopped after the graceful quit");

  std::printf("\n%d checks, %d failures\n", gChecks, gFailures);
  std::printf("%s\n", gFailures == 0 ? "coralcore_demo_event_loop: PASS" : "coralcore_demo_event_loop: FAIL");
  return gFailures == 0 ? 0 : 1;
}
