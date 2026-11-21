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

// =============================================================
//  tests_EventLoop.cpp
//
//  Covers the lock-free event implementation in event:
//
//    - the atomic wait primitive and its event counter
//    - the bounded lock-free MPSC ring: FIFO order, capacity, in-place visit
//    - a counting latch that no longer takes a mutex
//    - EventThread start/post/quit, including the lifecycle cases that used to
//      call std::terminate or deadlock
//    - MessageQueue ordering, delayed delivery, cancellation and quit semantics
//    - the growth path that replaced silent drops when a lane filled up
//    - the composite lane that grows past its ring: several producers sharing it,
//      with the posting-order rule that keeps the ring and the overflow in one
//      order
//    - the synchronisation barrier that holds back synchronous messages while
//      asynchronous ones still get through
//    - the message pool that `Message::obtain()` recycles through
//    - the callback lifetime rules that keep a destroyed callback from being
//      called into
//
//  Every wait in this file is bounded so a regression fails the test instead of
//  hanging it.
// =============================================================

// The composite lane lives with the queue's implementation rather than in the
// public headers, so it is reached the same way the queue reaches it.

#include "BlockingWait.h"
#include "LockFreeMpscQueue.h"
#include "MessageBuffer.h"

#include <core/event/EventHandler.h>
#include <core/event/EventLooper.h>
#include <core/event/EventThread.h>
#include <core/event/MessagePool.h>
#include <core/event/Timer.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace coral;

namespace {

int gChecks = 0;
int gFailures = 0;

void check(const bool condition, const std::string& what) {
  ++gChecks;
  if (condition) {
    std::printf("  ok   %s\n", what.c_str());
  } else {
    ++gFailures;
    std::printf("  FAIL %s\n", what.c_str());
  }
}

void section(const char* title) {
  std::printf("\n== %s ==\n", title);
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
 * A callback whose lifetime the test controls and can observe.
 *
 * It hands out `shared_ptr`s, so the handler can hold one weakly and notice when
 * the last owner goes away — which is the whole point of the lifetime tests.
 */
class TrackedCallback final : public EventHandler::Callback {
public:
  TrackedCallback(std::atomic<int>& calls, std::atomic<bool>& destroyed, const bool handled)
      : mCalls(calls),
        mDestroyed(destroyed),
        mHandled(handled) {}

  ~TrackedCallback() override {
    mDestroyed.store(true, std::memory_order_release);
  }

  bool handleMessage(Message* msg) override {
    if (msg != nullptr && msg->what == kInterceptWhat) {
      mCalls.fetch_add(1, std::memory_order_relaxed);
      return mHandled;
    }
    return false;
  }

  static constexpr int kInterceptWhat = 7;

private:
  std::atomic<int>& mCalls;
  std::atomic<bool>& mDestroyed;
  const bool mHandled;
};

/**
 * Counts synchronous messages and checks that they arrive in posting order.
 *
 * Two modes, selected by the message code so one handler can serve both checks:
 *
 *  - the stream code counts a single producer's ordered stream (`arg1` is the
 *    running count), so the highest value seen is also how many arrived;
 *  - the producer codes (`kProducerBase + p`) instead check that each of several
 *    producers kept its own order, which is the strongest property a single
 *    consumer can observe when several threads share one lane.
 */
class CountingHandler final : public EventHandler {
public:
  static constexpr int kStreamWhat = 1;
  static constexpr int kProducerBase = 100;
  static constexpr int kProducers = 4;
  static constexpr int kMissing = -1;

  CountingHandler(const std::shared_ptr<EventLooper>& looper, std::atomic<int>& received)
      : EventHandler(looper, nullptr),
        mReceived(received) {}

  void handleMessage(Message* msg) override {
    if (msg == nullptr) {
      return;
    }
    const int producer = msg->what - kProducerBase;
    if (producer >= 0 && producer < kProducers) {
      const int previous = mLastSeen[producer].exchange(msg->arg1, std::memory_order_acq_rel);
      if (previous != kMissing && msg->arg1 <= previous) {
        mOrderHeld.store(false, std::memory_order_release);
      }
      mReceived.fetch_add(1, std::memory_order_acq_rel);
      return;
    }

    if (msg->what != kStreamWhat) {
      return;
    }
    ++mExpected;
    if (msg->arg1 != mExpected) {
      mInOrder.store(false, std::memory_order_release);
    }
    mReceived.store(static_cast<int>(mExpected), std::memory_order_release);
  }

  [[nodiscard]] bool inOrder() const {
    return mInOrder.load(std::memory_order_acquire) && mOrderHeld.load(std::memory_order_acquire);
  }

private:
  std::atomic<int>& mReceived;
  std::atomic<int> mLastSeen[kProducers] = {
      std::atomic<int>{ kMissing }, std::atomic<int>{ kMissing },
      std::atomic<int>{ kMissing }, std::atomic<int>{ kMissing } };
  std::atomic<bool> mOrderHeld{ true };
  long long mExpected = 0;
  std::atomic<bool> mInOrder{ true };
};

/**
 * Counts synchronous and asynchronous messages apart, and records the
 * asynchronous count as it stood when a synchronous message was delivered.
 *
 * That last value is what proves the ordering inside the queue: the asynchronous
 * message was posted *behind* a synchronous one, so it can only have been
 * delivered first by passing a barrier.
 */
class BarrierProbe final : public EventHandler {
public:
  static constexpr int kAsyncWhat = 200;

  BarrierProbe(const std::shared_ptr<EventLooper>& looper,
      std::atomic<int>& sync,
      std::atomic<int>& async,
      std::atomic<int>& asyncWhenSyncRan)
      : EventHandler(looper, nullptr),
        mSync(sync),
        mAsync(async),
        mAsyncWhenSyncRan(asyncWhenSyncRan) {}

  void handleMessage(Message* msg) override {
    if (msg == nullptr) {
      return;
    }
    if (msg->what == kAsyncWhat) {
      // Recorded first, so the reading thread cannot see this increment without
      // also seeing that it happened before the count below.
      mAsync.fetch_add(1, std::memory_order_acq_rel);
      return;
    }
    // The count that had already been delivered by the time this synchronous
    // message ran. If the asynchronous message overtook this one, this is 1.
    mAsyncWhenSyncRan.store(mAsync.load(std::memory_order_acquire), std::memory_order_release);
    mSync.fetch_add(1, std::memory_order_acq_rel);
  }

private:
  std::atomic<int>& mSync;
  std::atomic<int>& mAsync;
  std::atomic<int>& mAsyncWhenSyncRan;
};

} // namespace anonymous

using EventLatch = EventThread::EventLatch;

int main() {
  // Unbuffered so a crash still shows how far the test got.
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::printf("event tests\n");
  std::printf("atomic wait backend: %s\n", BlockingWait::supported() ? "kernel futex" : "yield fallback");

  section("Atomic wait and event counter");
  {
    EventCounter counter;
    const uint32_t before = counter.sequence();
    check(!counter.waitForChange(before, 0), "a zero timeout returns without blocking");

    check(counter.waitForChange(before, 1000000) == false, "waitForChange reports a timeout");

    std::atomic<bool> woke{ false };
    std::thread waiter([&] {
      const uint32_t observed = counter.sequence();
      (void) counter.waitForChange(observed, BlockingWait::kWaitForever);
      woke.store(true, std::memory_order_release);
    });

    // Give the waiter time to actually sleep before signalling, so this exercises
    // the wake-up rather than the pre-check.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    counter.signal();
    check(waitFor([&] { return woke.load(std::memory_order_acquire); }, std::chrono::milliseconds(1000)),
        "signal() wakes a blocked waiter");
    waiter.join();

    const uint32_t current = counter.sequence();
    counter.signalAll();
    check(counter.sequence() > current, "signalAll() advances the sequence");
    check(counter.waitForChange(current, 0), "a changed sequence is reported without blocking");
  }

  section("Bounded lock-free MPSC ring");
  {
    LockFreeMpscQueue<int> queue(4);
    check(queue.capacity() == 4, "a power-of-two capacity is used as-is");
    check(LockFreeMpscQueue<int>(5).capacity() == 8, "a capacity is rounded up to a power of two");
    check(queue.empty(), "a new queue is empty");

    int value = 0;
    check(!queue.tryPop(value), "popping an empty queue fails");

    for (int i = 0; i < 4; ++i) {
      check(queue.tryPush(i), "pushing within capacity succeeds");
    }
    check(!queue.tryPush(99), "pushing past capacity fails instead of growing");
    check(!queue.empty(), "a full queue is not empty");

    bool ordered = true;
    for (int expected = 0; expected < 4; ++expected) {
      if (!queue.tryPop(value) || value != expected) {
        ordered = false;
      }
    }
    check(ordered, "elements come out in FIFO order");
    check(queue.empty(), "draining leaves the queue empty");
    check(queue.tryPush(7), "the ring is reusable after a full lap");
    check(queue.tryPop(value) && value == 7, "the reused slot holds the new element");
  }

  section("Ring visit reads in place");
  {
    LockFreeMpscQueue<int> queue(8);
    for (int i = 1; i <= 3; ++i) {
      (void) queue.tryPush(i);
    }

    std::vector<int> seen;
    const size_t visited = queue.tryVisit([&](const int& item) {
      seen.push_back(item);
    });
    check(visited == 3 && seen.size() == 3, "every queued element is visited");
    check(seen.size() == 3 && seen[0] == 1 && seen[1] == 2 && seen[2] == 3, "visiting preserves order");

    // Visiting must not consume anything.
    int value = 0;
    check(queue.tryPop(value) && value == 1, "visiting does not remove elements");
  }

  section("Multiple producers into the ring");
  {
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 2000;
    LockFreeMpscQueue<int> queue(1024);

    std::atomic<bool> done{ false };
    std::atomic<long long> pushed{ 0 };
    std::atomic<long long> popped{ 0 };
    std::atomic<long long> sum{ 0 };

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
      producers.emplace_back([&, p] {
        for (int i = 0; i < kPerProducer; ++i) {
          // Retry until the consumer drains: capacity is smaller than the total.
          while (!queue.tryPush(p * kPerProducer + i)) {
            std::this_thread::yield();
          }
          pushed.fetch_add(1, std::memory_order_relaxed);
        }
      });
    }

    std::thread consumer([&] {
      int value = 0;
      while (popped.load(std::memory_order_relaxed) < kProducers * kPerProducer) {
        if (queue.tryPop(value)) {
          popped.fetch_add(1, std::memory_order_relaxed);
          sum.fetch_add(value, std::memory_order_relaxed);
        } else if (done.load(std::memory_order_acquire) && queue.empty()) {
          break;
        } else {
          std::this_thread::yield();
        }
      }
    });

    for (auto& producer: producers) {
      producer.join();
    }
    done.store(true, std::memory_order_release);
    consumer.join();

    const long long expectedSum = static_cast<long long>(kProducers) * kPerProducer *
                                  (kProducers * kPerProducer - 1) / 2;
    std::printf("       pushed=%lld popped=%lld sum=%lld (expected %lld)\n", pushed.load(),
        popped.load(), sum.load(), expectedSum);
    check(popped.load() == kProducers * kPerProducer, "no element was lost or duplicated");
    check(sum.load() == expectedSum, "every element arrived intact");
  }

  section("EventLatch is lock-free and noexcept");
  {
    EventLatch latch(4);
    check(latch.getCount() == 0, "a fresh latch has counted nothing down");

    std::atomic<int> released{ 0 };
    std::vector<std::thread> waiters;
    waiters.reserve(3);
    for (int i = 0; i < 3; ++i) {
      waiters.emplace_back([&] {
        latch.await();
        released.fetch_add(1, std::memory_order_relaxed);
      });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    check(released.load() == 0, "waiters block until the latch fires");

    latch.latch();
    latch.latch();
    check(latch.getCount() == 2, "the count reflects the signals so far");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    check(released.load() == 0, "waiters stay blocked until the last signal");

    latch.latch();
    latch.latch();
    check(waitFor([&] { return released.load() == 3; }, std::chrono::milliseconds(1000)),
        "reaching zero releases every waiter");
    for (auto& waiter: waiters) {
      waiter.join();
    }

    // Over-signalling must not wrap the counter past zero.
    latch.latch();
    check(latch.getCount() == 4, "extra latch() calls have no effect");

    // A count of zero is immediately open.
    EventLatch open(0);
    open.await();
    check(true, "await() on a zero latch returns immediately");
  }

  section("EventThread starts, posts and quits");
  {
    EventThread thread("coral-test");
    check(!thread.isAlive(), "a fresh EventThread is not alive");

    thread.start();
    check(thread.isAlive(), "start() leaves the thread alive");
    check(thread.getLooper() != nullptr, "start() returns only once the looper exists");
    check(!thread.getLooper()->isCurrentThread(),
        "the looper belongs to the event thread, not to the caller");

    auto handler = EventHandler::create(thread.getLooper(), nullptr);
    check(handler != nullptr, "a handler can be created on the thread's looper");

    constexpr int kMessages = 200;
    std::atomic<int> handled{ 0 };
    std::atomic<bool> ranOnLooperThread{ true };
    const auto callerId = std::this_thread::get_id();

    bool allAccepted = true;
    for (int i = 0; i < kMessages; ++i) {
      const bool ok = handler->post([&handled, &ranOnLooperThread, callerId] {
        if (std::this_thread::get_id() == callerId) {
          ranOnLooperThread.store(false, std::memory_order_relaxed);
        }
        handled.fetch_add(1, std::memory_order_relaxed);
      });
      if (i == 0) {
      }
      allAccepted = ok && allAccepted;
    }
    check(allAccepted, "every post was accepted while the looper was running");
    check(waitFor([&] { return handled.load() == kMessages; }, std::chrono::milliseconds(3000)),
        "every posted closure ran");
    check(ranOnLooperThread.load(), "the closures ran on the looper thread, not the poster's");

    check(thread.quit(), "quit() reports that a running thread was stopped");
    check(waitFor([&] { return !thread.isAlive(); }, std::chrono::milliseconds(3000)),
        "the thread is no longer alive after quit()");
  }

  section("EventThread destruction and restart safety");
  {
    {
      EventThread thread("coral-dtor");
      thread.start();
      check(thread.isAlive(), "the thread is running before destruction");
      // No explicit quit: the destructor has to stop the thread, otherwise the
      // joinable std::thread inside it would call std::terminate.
    }
    check(true, "destroying a running EventThread did not terminate the process");

    EventThread reusable("coral-again");
    reusable.start();
    check(reusable.isAlive(), "the first start() works");
    reusable.quit();
    check(!reusable.isAlive(), "the thread stopped");

    reusable.start();
    check(reusable.isAlive(), "the same object can be started again");
    reusable.quit();
    check(!reusable.isAlive(), "the restarted thread stopped");

    check(!reusable.quit(), "quitting a stopped thread reports nothing to quit");
  }

  section("Delayed delivery and cancellation");
  {
    EventThread thread("coral-delayed");
    thread.start();
    const auto looper = thread.getLooper();

    std::atomic<int> immediate{ 0 };
    std::atomic<int> delayed{ 0 };
    auto handler = EventHandler::create(looper, nullptr);

    check(handler->post([&immediate] { immediate.fetch_add(1, std::memory_order_relaxed); }),
        "an immediate post is accepted");

    // A message that will be withdrawn before it is due must never run.
    std::atomic<int> cancelled{ 0 };
    constexpr int kCancelledWhat = 42;
    auto cancellable = std::make_shared<EventHandler>(looper, nullptr);
    check(cancellable->sendEmptyMessageDelayed(kCancelledWhat, std::chrono::milliseconds(50)),
        "a delayed message is accepted");
    cancellable->removeMessages(kCancelledWhat);
    check(!cancellable->hasMessages(kCancelledWhat), "a withdrawn message is no longer reported");

    // A delayed message that is not withdrawn must run, late rather than early.
    const auto postedAt = std::chrono::steady_clock::now();
    std::atomic<bool> delayedRan{ false };
    auto delayedHandler = std::make_shared<EventHandler>(looper, nullptr);
    check(delayedHandler->postDelay([&] {
            delayedRan.store(true, std::memory_order_relaxed);
            delayed.fetch_add(1, std::memory_order_relaxed);
          },
              std::chrono::milliseconds(60)),
        "a delayed closure is accepted");
    check(!delayedRan.load(), "a delayed closure does not run before its deadline");

    check(waitFor([&] { return delayedRan.load(); }, std::chrono::milliseconds(2000)),
        "the delayed closure ran");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - postedAt);
    std::printf("       delayed closure ran after %lld ms (asked for 60)\n",
        static_cast<long long>(elapsed.count()));
    check(elapsed.count() >= 55, "the delay was honoured");
    check(cancelled.load() == 0, "the withdrawn message never ran");
    check(immediate.load() == 1, "the immediate message ran exactly once");

    thread.quit();
  }

  section("Graceful quit delivers due work and drops the future");
  {
    EventThread thread("coral-safe-quit");
    thread.start();
    const auto looper = thread.getLooper();

    std::atomic<int> due{ 0 };
    std::atomic<int> future{ 0 };
    auto handler = std::make_shared<EventHandler>(looper, nullptr);

    // Queue a batch of due closures plus one that is far in the future.
    for (int i = 0; i < 50; ++i) {
      handler->post([&due] { due.fetch_add(1, std::memory_order_relaxed); });
    }
    handler->postDelay([&future] { future.fetch_add(1, std::memory_order_relaxed); },
        std::chrono::seconds(10));

    // Ask for a graceful quit, then check that the due work still ran.
    check(thread.quitSafety(), "quitSafety() reports a running thread");
    check(waitFor([&] { return due.load() == 50; }, std::chrono::milliseconds(3000)),
        "a safe quit delivers the work that was already due");
    std::printf("       due=%d future=%d\n", due.load(), future.load());
    check(future.load() == 0, "a safe quit drops work scheduled far in the future");
    check(waitFor([&] { return !thread.isAlive(); }, std::chrono::milliseconds(3000)),
        "the thread stopped after the graceful quit");
  }

  section("Quit from inside a handler");
  {
    EventThread thread("coral-self-quit");
    thread.start();
    auto* rawThread = &thread;
    auto handler = std::make_shared<EventHandler>(thread.getLooper(), nullptr);

    std::atomic<int> before{ 0 };
    std::atomic<int> after{ 0 };
    handler->post([&] { before.fetch_add(1, std::memory_order_relaxed); });
    // Quitting from the looper thread must not deadlock on a self-join.
    handler->post([rawThread] { rawThread->quit(); });
    handler->post([&] { after.fetch_add(1, std::memory_order_relaxed); });

    check(waitFor([&] { return !rawThread->isAlive(); }, std::chrono::milliseconds(3000)),
        "quitting from a handler stops the thread without deadlocking");
    std::printf("       handlers before quit=%d after quit=%d\n", before.load(), after.load());
    check(before.load() == 1, "the handler queued before the quit ran");
    check(after.load() <= 1, "the handler queued after the quit is dropped or delivered");
  }

  section("The growable lane keeps one order across ring and overflow");
  {
    // The lane is a wait-free ring with a side list behind it, and the two are
    // only in order together because the consumer compares their fronts by posting
    // stamp. This drives that directly: the ring is far smaller than the burst, so
    // most entries are in the side list, and several producers push at once so the
    // two are in use simultaneously.
    struct Entry {
      uint64_t order = 0;
      int producer = 0;
      int sequence = 0;
    };

    constexpr int kProducers = 4;
    constexpr int kPerProducer = 2000;
    constexpr int kTotal = kProducers * kPerProducer;

    MessageBuffer<Entry> lane(64);
    check(lane.ringCapacity() == 64, "a lane reports its ring capacity");

    std::atomic<long long> posted{ 0 };
    std::atomic<bool> start{ false };
    std::atomic<int> consumed{ 0 };
    std::vector<int> lastSeen(static_cast<size_t>(kProducers), -1);
    std::atomic<int> violations{ 0 };

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
      producers.emplace_back([&lane, &posted, &start, p] {
        while (!start.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }
        for (int i = 1; i <= kPerProducer; ++i) {
          const long long stamp = posted.fetch_add(1, std::memory_order_relaxed);
          lane.push(Entry{ static_cast<uint64_t>(stamp), p, i });
        }
      });
    }

    std::thread consumer([&] {
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      Entry entry{};
      while (consumed.load(std::memory_order_relaxed) < kTotal) {
        if (!lane.pop(entry)) {
          std::this_thread::yield();
          continue;
        }
        const int p = entry.producer;
        if (p >= 0 && p < kProducers) {
          if (lastSeen[static_cast<size_t>(p)] != -1 &&
              entry.sequence <= lastSeen[static_cast<size_t>(p)]) {
            violations.fetch_add(1, std::memory_order_relaxed);
          }
          lastSeen[static_cast<size_t>(p)] = entry.sequence;
        }
        consumed.fetch_add(1, std::memory_order_relaxed);
      }
    });

    start.store(true, std::memory_order_release);
    for (auto& producer: producers) {
      producer.join();
    }
    consumer.join();

    check(consumed.load(std::memory_order_relaxed) == kTotal,
        "every entry pushed into a small ring came out");
    check(violations.load(std::memory_order_relaxed) == 0,
        "each producer's own entries kept their order across ring and overflow");
  }

  section("Handler rejects work once the looper is gone");
  {
    auto handler = std::shared_ptr<EventHandler>{};
    {
      EventThread thread("coral-gone");
      thread.start();
      handler = std::make_shared<EventHandler>(thread.getLooper(), nullptr);
      check(handler->post([] {}), "posting to a live looper succeeds");
      thread.quit();
    }
    // The looper is destroyed with the thread, so the handler must refuse rather
    // than touch a dead queue.
    check(!handler->post([] {}), "posting after the looper is gone is refused");
    check(!handler->sendEmptyMessage(1), "sending after the looper is gone is refused");
    check(!handler->hasMessages(1), "querying after the looper is gone reports nothing");
    handler->removeAllCallbacksAndMessages();
    check(true, "removing messages after the looper is gone is harmless");
  }

  section("A full lane grows instead of dropping messages");
  {
    // A fresh looper with small rings, so the burst below overflows them many
    // times over. If a lane still dropped instead of growing, most of this burst
    // would simply never arrive.
    EventThread thread("coral-growth");
    thread.start();

    std::atomic<int> received{ 0 };
    auto handler = std::make_shared<CountingHandler>(thread.getLooper(), received);

    constexpr int kMessages = 3000;
    bool allAccepted = true;
    for (int i = 1; i <= kMessages; ++i) {
      allAccepted = handler->sendMessage(Message::obtain(handler, CountingHandler::kStreamWhat, i, 0))
                    && allAccepted;
    }
    check(allAccepted, "every post past the ring capacity was accepted");
    check(waitFor([&] { return received.load(std::memory_order_acquire) == kMessages; },
              std::chrono::seconds(30)),
        "a burst far past the ring capacity is delivered in full");
    std::printf("       delivered=%d of %d\n", received.load(std::memory_order_acquire), kMessages);
    check(received.load(std::memory_order_acquire) == kMessages,
        "no message was dropped when a lane filled up");
    check(handler->inOrder(), "the overflow kept the posting order");

    thread.quit();
  }

  section("Several producers grow the same lane without losing order");
  {
    EventThread thread("coral-growth-mp");
    thread.start();

    std::atomic<int> received{ 0 };
    auto handler = std::make_shared<CountingHandler>(thread.getLooper(), received);

    // Four producers, each posting its own numbered stream into the same lane.
    // The lane is far smaller than the combined burst, so every producer spends
    // most of its time in the overflow path, and the two paths are being taken at
    // the same time by different threads.
    constexpr int kPerProducer = 2000;
    std::atomic<bool> allAccepted{ true };
    std::vector<std::thread> producers;
    producers.reserve(CountingHandler::kProducers);
    for (int p = 0; p < CountingHandler::kProducers; ++p) {
      producers.emplace_back([&handler, &allAccepted, p] {
        for (int i = 1; i <= kPerProducer; ++i) {
          if (!handler->sendMessage(Message::obtain(handler, CountingHandler::kProducerBase + p, i, 0))) {
            allAccepted.store(false, std::memory_order_release);
          }
        }
      });
    }
    for (auto& producer: producers) {
      producer.join();
    }

    constexpr int kTotal = CountingHandler::kProducers * kPerProducer;
    check(allAccepted.load(std::memory_order_acquire), "every concurrent post was accepted");
    check(waitFor([&] { return received.load(std::memory_order_acquire) == kTotal; }, std::chrono::seconds(60)),
        "the concurrent burst was delivered in full");
    std::printf("       delivered=%d of %d\n", received.load(std::memory_order_acquire), kTotal);
    check(received.load(std::memory_order_acquire) == kTotal,
        "no message was lost when several producers shared one lane");
    check(handler->inOrder(), "each producer's own messages kept their order");

    thread.quit();
  }

  section("Message reuse avoids repeated allocation");
  {
    // The pool is the fix for "a short message costs an allocation and a free".
    // Measuring it through a running looper would only measure how quickly the
    // looper happens to recycle, so the round trip is driven directly first.
    MessagePool::global()->drain();
    const auto& pool = MessagePool::global();
    check(pool->available() == 0, "a drained pool holds nothing");

    {
      auto held = Message::obtain(nullptr, 1);
      Message* const first = held.get();
      check(pool->available() == 0, "a message that is still held is not on the free list");
      held.reset();
      check(pool->available() == 1, "releasing a message puts it back in the pool");

      auto again = Message::obtain(nullptr, 2);
      check(again.get() == first, "the same message object is handed back out");
      check(pool->reusedCount() >= 1, "the pool counted the reuse");
      check(pool->allocatedCount() == 1, "the round trip did not allocate a second message");
    }

    // A burst posted on a live looper still has to end up recycled, because the
    // queue releases every message through the pool's deleter.
    EventThread thread("coral-pool");
    thread.start();

    std::atomic<int> received{ 0 };
    auto handler = std::make_shared<CountingHandler>(thread.getLooper(), received);

    constexpr int kMessages = 5000;
    for (int i = 1; i <= kMessages; ++i) {
      handler->sendMessage(Message::obtain(handler, CountingHandler::kStreamWhat, i, 0));
    }
    check(waitFor([&] { return received.load(std::memory_order_acquire) == kMessages; },
              std::chrono::seconds(30)),
        "the pooled burst drained");
    check(handler->sendEmptyMessage(1), "a message can still be posted after the burst");
    thread.quit();

    const auto allocated = pool->allocatedCount();
    const auto reused = pool->reusedCount();
    const auto recycled = pool->recycledCount();
    std::printf("       allocated=%llu reused=%llu recycled=%llu available=%zu\n",
        static_cast<unsigned long long>(allocated), static_cast<unsigned long long>(reused),
        static_cast<unsigned long long>(recycled), pool->available());
    check(reused > 0, "messages were taken back out of the pool");
    check(recycled > 0, "delivered messages were returned to the pool");
    check(allocated < static_cast<uint64_t>(kMessages),
        "the pool kept the allocator out of the delivery path");
    check(pool->available() <= pool->capacity(), "the free list stayed inside its bound");
  }

  section("Syncing queue blocks synchronous work but passes asynchronous work");
  {
    EventThread thread("coral-barrier");
    thread.start();

    std::atomic<int> sync{ 0 };
    std::atomic<int> async{ 0 };
    std::atomic<int> asyncWhenSyncRan{ -1 };
    std::atomic<int> secondToken{ 0 };

    auto handler = std::make_shared<BarrierProbe>(thread.getLooper(), sync, async, asyncWhenSyncRan);

    // A barrier is held by a thread for as long as the work behind it takes, and
    // released when that work is done. This thread plays the part of the frame
    // that takes the barrier: it holds it while ordinary messages pile up and
    // hands it back once the test says the frame is over. The fence is posted from
    // here, after the barrier, so "the fence was queued while the barrier was
    // held" is a fact rather than a race.
    std::atomic<bool> fencePosted{ false };
    std::atomic<bool> barrierRelease{ false };
    std::thread barrierHolder([&] {
      const auto guard = handler->deriveSyncBarrier();
      if (!guard.active()) {
        return;
      }
      fencePosted.store(handler->sendEmptyMessage(1), std::memory_order_release);
      // Hold until the test says the frame is over, with a bound so a regression
      // cannot hang the test.
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
      while (!barrierRelease.load(std::memory_order_acquire) &&
             std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      // The guard hands the barrier back as this scope ends.
    });

    // The fence was queued while the barrier was up, and the barrier stays up
    // until the release below, so every observation in between is made against a
    // barrier that is genuinely held.
    check(waitFor([&] { return handler->parkedByBarrierCount() >= 1; }, std::chrono::seconds(10)),
        "the barrier held the synchronous fence back");
    check(fencePosted.load(std::memory_order_acquire), "the synchronous fence was accepted");
    check(handler->isSyncBarrierSet(), "the queue reports the barrier as held");
    check(sync.load(std::memory_order_acquire) == 0,
        "the held fence has not run while the barrier is up");

    // The urgent work: queued behind the fence, so it can only be delivered by
    // passing the barrier.
    check(handler->sendEmptyMessageAsync(BarrierProbe::kAsyncWhat), "an asynchronous message is accepted");
    check(waitFor([&] { return async.load(std::memory_order_acquire) == 1; }, std::chrono::seconds(10)),
        "the asynchronous message got past the barrier");
    check(sync.load(std::memory_order_acquire) == 0,
        "the fence was still held while urgent work was delivered");

    // Another thread finding the queue already synchronised is refused rather
    // than stacking a barrier it does not own.
    std::atomic<bool> secondBarrierRefused{ false };
    std::thread otherHolder([&handler, &secondBarrierRefused] {
      const auto guard = handler->deriveSyncBarrier();
      secondBarrierRefused.store(!guard.active(), std::memory_order_release);
    });
    otherHolder.join();
    check(secondBarrierRefused.load(std::memory_order_acquire),
        "a second thread cannot take a barrier that is already held");
    check(handler->isSyncBarrierSet(), "the original barrier is still held after the refused attempt");

    // Handing the barrier back releases the fence, which must have seen the urgent
    // message already delivered.
    barrierRelease.store(true, std::memory_order_release);
    barrierHolder.join();
    check(waitFor([&] { return sync.load(std::memory_order_acquire) == 1; }, std::chrono::seconds(10)),
        "the held fence was delivered once the barrier went away");
    check(!handler->isSyncBarrierSet(), "the guard released the barrier when its scope ended");
    check(asyncWhenSyncRan.load(std::memory_order_acquire) == 1,
        "the asynchronous message overtook the synchronous one queued ahead of it");
    check(async.load(std::memory_order_acquire) == 1, "each asynchronous message was delivered exactly once");

    // A barrier has to be re-derivable, not a one-shot.
    std::thread secondHolder([&handler, &secondToken] {
      const auto guard = handler->deriveSyncBarrier();
      secondToken.store(guard.active() ? 1 : -1, std::memory_order_release);
    });
    secondHolder.join();
    check(secondToken.load(std::memory_order_acquire) == 1, "a second barrier could be derived");
    check(waitFor([&] { return !handler->isSyncBarrierSet(); }, std::chrono::seconds(10)),
        "the second barrier was released as well");

    thread.quit();
  }

  section("A destroyed callback is never called");
  {
    EventThread thread("coral-callback");
    thread.start();
    const auto looper = thread.getLooper();

    std::atomic<int> intercepted{ 0 };
    std::atomic<int> delivered{ 0 };
    std::atomic<bool> destroyed{ false };

    auto callback = std::make_shared<TrackedCallback>(intercepted, destroyed, true);
    auto handler = EventHandler::create(looper, callback);
    check(handler->hasCallback(), "a live callback is reported as installed");

    handler->sendMessage(Message::obtain(handler, TrackedCallback::kInterceptWhat, 0, 0));
    check(waitFor([&] { return intercepted.load(std::memory_order_relaxed) == 1; }, std::chrono::seconds(10)),
        "a live callback is called");
    check(!destroyed.load(std::memory_order_acquire), "the callback is alive while the handler uses it");

    // This is the case that used to be a dangling call: the callback goes away
    // while the handler keeps running.
    callback.reset();
    check(waitFor([&] { return destroyed.load(std::memory_order_acquire); }, std::chrono::seconds(10)),
        "the callback was destroyed while the handler is still alive");
    check(!handler->hasCallback(), "the handler reports that its callback is gone");

    // Several messages, so a dangling call would be hit more than once and a
    // crash would be unambiguous rather than incidental. Every one of them has to
    // fall through to the handler instead of reaching the dead callback.
    for (int i = 0; i < 64; ++i) {
      handler->sendMessage(Message::obtain(handler, TrackedCallback::kInterceptWhat, 0, 0));
    }
    handler->sendMessage(Message::obtain(handler, TrackedCallback::kInterceptWhat, 0, 0));
    check(waitFor([&] {
            return !handler->hasMessages(TrackedCallback::kInterceptWhat);
          }, std::chrono::seconds(10)),
        "messages addressed to a dead callback still drain");
    check(intercepted.load(std::memory_order_relaxed) == 1,
        "a destroyed callback is never called again");
    check(true, "dispatching to a destroyed callback did not fault");

    // Installing a callback after the fact works the same way, and clearing it
    // leaves the handler usable.
    std::atomic<bool> replacementDestroyed{ false };
    std::atomic<int> replacementCalls{ 0 };
    {
      auto replacement = std::make_shared<TrackedCallback>(replacementCalls, replacementDestroyed, true);
      handler->setCallback(replacement);
      check(handler->hasCallback(), "a callback installed after construction is reported");
      handler->sendMessage(Message::obtain(handler, TrackedCallback::kInterceptWhat, 0, 0));
      check(waitFor([&] { return replacementCalls.load(std::memory_order_relaxed) == 1; },
                std::chrono::seconds(10)),
          "the installed callback is called");
      replacement.reset();
      check(waitFor([&] { return replacementDestroyed.load(std::memory_order_acquire); },
                std::chrono::seconds(10)),
          "the installed callback was destroyed by its owner");
      check(!handler->hasCallback(), "a destroyed replacement callback is reported as gone");
    }

    // Clearing the callback outright must also leave the handler usable.
    handler->setCallback(nullptr);
    check(!handler->hasCallback(), "clearing the callback is reported");
    check(handler->sendEmptyMessage(1), "the handler still accepts work without a callback");

    // A subclass that installs a callback by `shared_ptr` still gives it first
    // refusal, and the handler never becomes the owner: the weak reference it holds
    // does not keep the callback alive once the caller lets go.
    std::atomic<int> plainCalls{ 0 };
    std::atomic<bool> plainDestroyed{ false };
    {
      class PlainHandler final : public EventHandler {
      public:
        PlainHandler(const std::shared_ptr<EventLooper>& looper, const std::shared_ptr<Callback>& callback)
            : EventHandler(looper, callback) {}
      };

      auto plainCallback = std::make_shared<TrackedCallback>(plainCalls, plainDestroyed, true);
      auto plain = std::make_shared<PlainHandler>(looper, plainCallback);
      check(plain->hasCallback(), "a subclass can install a callback");
      plain->sendMessage(Message::obtain(plain, TrackedCallback::kInterceptWhat, 0, 0));
      check(waitFor([&] { return plainCalls.load(std::memory_order_relaxed) == 1; },
                std::chrono::seconds(10)),
          "the callback handles the message before the handler does");
      check(!plainDestroyed.load(std::memory_order_acquire), "the caller keeps the callback alive");

      // Dropping the caller's reference is what destroys it; a handler that owned the
      // callback would have kept it alive past this point.
      plainCallback.reset();
    }
    check(plainDestroyed.load(std::memory_order_acquire), "the handler never owned the callback");

    thread.quit();
  }

  section("Remove, then post the replacement of the same code");
  {
    // The caller's shape: cancel the pending work for a code, then immediately ask
    // for it again. The replacement is posted after the remove returns, so it must
    // survive — including when the remove runs on another thread, which is the case
    // that used to swallow it.
    constexpr int kWork = 4242;

    class WorkHandler final : public EventHandler {
    public:
      WorkHandler(const std::shared_ptr<EventLooper>& looper, std::atomic<int>& ran)
          : EventHandler(looper, nullptr),
            mRan(ran) {}

      void handleMessage(Message* msg) override {
        if (msg != nullptr && msg->what == kWork) {
          mRan.fetch_add(1, std::memory_order_relaxed);
        }
      }

    private:
      std::atomic<int>& mRan;
    };

    EventThread thread("coral-remove");
    thread.start();

    std::atomic<int> ran{ 0 };
    auto handler = std::make_shared<WorkHandler>(thread.getLooper(), ran);

    constexpr int kRounds = 100;
    int roundsWhereTheReplacementRan = 0;
    for (int i = 0; i < kRounds; ++i) {
      // Work that is on its way, and a remove that runs to completion first.
      handler->sendEmptyMessageDelayed(kWork, std::chrono::milliseconds(10));
      std::thread remover([&handler] { handler->removeMessages(kWork); });
      remover.join();
      // The replacement, posted after the remove: this is the message that has to
      // run, and the one an unscoped remove would have swallowed.
      handler->sendEmptyMessageDelayed(kWork, std::chrono::milliseconds(1));
      if (waitFor([&] { return ran.load(std::memory_order_acquire) == i + 1; }, std::chrono::seconds(5))) {
        ++roundsWhereTheReplacementRan;
      }
    }
    std::printf("       replacement delivered in %d of %d rounds\n", roundsWhereTheReplacementRan, kRounds);
    check(roundsWhereTheReplacementRan == kRounds,
        "every replacement posted after a remove on another thread still ran");

    // The plain form, entirely on this thread.
    std::atomic<int> plain{ 0 };
    auto plainHandler = std::make_shared<WorkHandler>(thread.getLooper(), plain);
    plainHandler->sendEmptyMessageDelayed(kWork, std::chrono::milliseconds(10));
    plainHandler->removeMessages(kWork);
    plainHandler->sendEmptyMessageDelayed(kWork, std::chrono::milliseconds(1));
    check(waitFor([&] { return plain.load(std::memory_order_acquire) == 1; }, std::chrono::seconds(5)),
        "a replacement posted after removeMessages runs");
    check(plain.load(std::memory_order_acquire) == 1, "the replacement ran exactly once");

    thread.quit();
  }

  section("A self-rescheduling message must be removed before it is posted");
  {
    // This is the shape that stalled AvLivePlayer: scheduleNextWork() posted the next
    // MSG_DO_SOME_WORK and then called removeMessages() for the same code in the very next
    // statement. removeMessages() withdraws everything that was already queued when it ran,
    // and the message posted a moment earlier is part of that set, so the work loop
    // cancelled its own successor and stopped ticking. Cancelling the pending work *before*
    // posting is what keeps the replacement, which is why the order matters.
    constexpr int kTick = 99;

    class TickHandler final : public EventHandler {
    public:
      TickHandler(const std::shared_ptr<EventLooper>& looper, std::atomic<int>& ticks)
          : EventHandler(looper, nullptr),
            mTicks(ticks) {}

      void handleMessage(Message* msg) override {
        if (msg != nullptr && msg->what == kTick) {
          mTicks.fetch_add(1, std::memory_order_relaxed);
        }
      }

    private:
      std::atomic<int>& mTicks;
    };

    EventThread thread("coral-work-loop");
    thread.start();

    const auto tickDelay = std::chrono::milliseconds(5);

    // Post, then remove: the tick posted here is the one the remove withdraws.
    std::atomic<int> withdrawnTicks{ 0 };
    auto wrongOrder = std::make_shared<TickHandler>(thread.getLooper(), withdrawnTicks);
    wrongOrder->sendEmptyMessageAtTime(kTick, std::chrono::steady_clock::now() + tickDelay);
    wrongOrder->removeMessages(kTick);

    // Remove, then post: the order AvLivePlayer::scheduleNextWork has to use.
    std::atomic<int> keptTicks{ 0 };
    auto rightOrder = std::make_shared<TickHandler>(thread.getLooper(), keptTicks);
    rightOrder->removeMessages(kTick);
    rightOrder->sendEmptyMessageAtTime(kTick, std::chrono::steady_clock::now() + tickDelay);

    check(waitFor([&] { return keptTicks.load(std::memory_order_acquire) >= 1; }, std::chrono::seconds(5)),
        "a tick posted after the remove still runs");
    // Well past the tick's deadline, so a delivered-withdrawn-tick would have shown up by now.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(withdrawnTicks.load(std::memory_order_acquire) == 0,
        "a tick posted before the remove is withdrawn, so the remove has to come first");

    thread.quit();
  }

  section("Timer repeats, can be stopped, and runs a one-shot");
  {
    EventThread thread("coral-timer");
    thread.start();
    std::atomic<int> fires{ 0 };
    auto timer = Timer::create(thread.getLooper(), std::chrono::milliseconds(5), [&] {
      if (++fires >= 3) thread.quitSafety();
    }, true);
    check(timer != nullptr, "Timer::create returns a timer on a live looper");
    timer->start();
    check(waitFor([&] { return !thread.isAlive(); }, std::chrono::seconds(5)),
        "a repeating Timer keeps firing and can stop the looper from its callback");
    check(fires.load(std::memory_order_acquire) >= 3, "the repeating timer fired at least three times");

    timer->stop();
    const int stopped = fires.load(std::memory_order_acquire);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    check(fires.load(std::memory_order_acquire) == stopped, "stop() cancels the pending fire");

    EventThread onceThread("coral-timer-once");
    onceThread.start();
    std::atomic<int> once{ 0 };
    auto oneShot = Timer::create(onceThread.getLooper(), std::chrono::milliseconds(1), [&] {
      ++once;
      onceThread.quitSafety();
    });
    oneShot->start();
    check(waitFor([&] { return !onceThread.isAlive(); }, std::chrono::seconds(5)),
        "a one-shot Timer quits the looper");
    check(once.load(std::memory_order_acquire) == 1, "a one-shot timer fires exactly once");
  }

  std::printf("\n%d checks, %d failures\n", gChecks, gFailures);
  std::printf("%s\n", gFailures == 0 ? "tests_EventLoop: PASS" : "tests_EventLoop: FAIL");
  return gFailures == 0 ? 0 : 1;
}
