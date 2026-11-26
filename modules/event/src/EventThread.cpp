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

#include <core/event/EventThread.h>

#include <core/event/EventHandler.h>
#include <core/event/EventLooper.h>

#include "BlockingWait.h"

#include <minilog/log.h>

#if defined(__APPLE__)
#include <pthread.h>
#elif defined(__linux__)
#include <pthread.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

#include <string_view>

namespace coral {

namespace {

void setThreadName(char const* name) noexcept {
#if defined(__linux__)
  pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
  pthread_setname_np(name);
#elif defined(_WIN32)
  std::string_view u8name(name);
  size_t size = MultiByteToWideChar(CP_UTF8, 0, u8name.data(), u8name.size(), nullptr, 0);

  std::wstring u16name;
  u16name.resize(size);
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8name.data(), u8name.size(), u16name.data(), u16name.size());

  SetThreadDescription(GetCurrentThread(), u16name.data());
#endif
}

} // namespace anonymous

/**
 * One generation of the thread.
 *
 * <p>The thread body captures this object instead of the `EventThread` itself, so a thread that is
 * detached because it was asked to quit from its own looper keeps touching only the state it owns,
 * never an `EventThread` that may already be gone. Every `start()` creates a new generation: a run
 * that is still unwinding cannot clear the looper a later generation has just published.
 */
struct EventThread::State {
  /** Guards `looper`, which is read by `getLooper()` and cleared when the run ends. */
  std::mutex mutex;
  /** The published looper, or null before the thread published it and after it stopped. */
  std::shared_ptr<EventLooper> looper;
  /** The looper thread's own id, published with the looper. */
  std::atomic<std::thread::id> threadId{};
  /** Fired once the looper and the id are published. */
  EventLatch barrier{1};
  /** Set by a `quit()` that arrives before the looper was published. */
  std::atomic_bool quitRequested{false};
  /** Set once the looper was deregistered and the published looper cleared. */
  std::atomic_bool runFinished{false};
};

EventThread::EventThread(std::string name) noexcept
    : threadName(std::move(name)) {
}

EventThread::~EventThread() {
  // A joinable std::thread destroyed without join or detach calls
  // std::terminate, and a looper thread left running would keep touching this
  // object after it is gone. Shut it down here so destruction is always safe.
  quit();
}

void EventThread::start() {
  std::shared_ptr<State> state;
  {
    std::lock_guard<std::mutex> guard(mThreadLock);
    if (mThread != nullptr) {
      return;
    }
    // Every start gets a new generation. A thread of an older generation may still be unwinding
    // (it was detached when it quit itself), and it must not clear the new generation's looper.
    state = std::make_shared<State>();
    mState = state;
    // The thread captures the state, not `this`: the state is what outlives the EventThread when
    // the thread was detached.
    mThread = std::make_shared<std::thread>([state, name = threadName] {
      runState(state, name);
    });
  }

  // Wait until the looper exists. The barrier is the handshake: `runState()` signals it once the
  // looper and the thread id are published, so a caller can use the looper and see the thread as
  // alive immediately after start() returns. The wait happens outside the lock on purpose: the
  // thread takes that lock to publish, so holding it here while waiting would deadlock.
  state->barrier.await();
}

void EventThread::releaseThread(const bool join) {
  // The handle is taken under the lock and then released, so joining happens without holding it.
  std::shared_ptr<std::thread> handle;
  {
    std::lock_guard<std::mutex> guard(mThreadLock);
    handle = std::move(mThread);
  }
  if (handle == nullptr) {
    return;
  }

  const bool joinable = handle->joinable();
  const bool self = handle->get_id() == std::this_thread::get_id();
  if (joinable && join && !self) {
    handle->join();
    return;
  }
  if (joinable) {
    // Either a detach was asked for, or this is the looper thread itself, where joining would
    // deadlock. Letting it finish on its own is the only option.
    handle->detach();
  }
}

bool EventThread::quit() {
  return requestQuit(false);
}

bool EventThread::quitSafety() {
  return requestQuit(true);
}

bool EventThread::requestQuit(const bool safety) {
  const auto state = currentState();
  std::shared_ptr<EventLooper> looper;
  if (state != nullptr) {
    std::lock_guard<std::mutex> guard(state->mutex);
    looper = state->looper;
  }

  if (looper == nullptr) {
    if (state == nullptr || state->runFinished.load(std::memory_order_acquire)) {
      // Never started, or the thread already stopped and cleared its looper.
      releaseThread(true);
      return false;
    }
    // The thread may still be between start() and publishing its looper; ask it to stop before
    // the join so that it cannot enter the loop and block the join forever.
    state->quitRequested.store(true, std::memory_order_release);
    releaseThread(true);
    return true;
  }

  if (safety) {
    looper->quitSafety();
  } else {
    looper->quit();
  }
  // Joining is only safe from another thread: when quit() is called from a handler running on the
  // looper thread, joining would deadlock.
  releaseThread(!looper->isCurrentThread());
  return true;
}

void EventThread::run() {
  // Public for compatibility with the original thread body. `start()` no longer routes through
  // here; it runs a generation with `runState()` so the body never captures the object.
  const auto state = currentState();
  if (state != nullptr) {
    runState(state, threadName);
  }
}

void EventThread::runState(const std::shared_ptr<State>& state, std::string name) {
  if (!name.empty()) {
    setThreadName(name.c_str());
  }

  EventLooper::prepare();
  MINILOG_DEBUG("The event thread '{}' prepared its looper", name);
  {
    // Publish the looper and the id together, under the state lock, so whoever start() is waiting
    // for sees a complete picture: a thread that is alive and has a usable looper.
    std::lock_guard<std::mutex> guard(state->mutex);
    state->looper = EventLooper::myLooper();
    state->threadId.store(std::this_thread::get_id(), std::memory_order_release);
  }
  state->barrier.latch();

  if (!state->quitRequested.load(std::memory_order_acquire)) {
    EventLooper::loop();
  }

  {
    const auto id = state->threadId.load(std::memory_order_acquire);
    if (id != std::thread::id{}) {
      EventLooper::remove(id);
    }
  }
  {
    // Clear the published looper before the id: a reader that sees the thread as stopped always
    // sees the looper as gone too, which is the contract `getLooper()` and `quit()` rely on.
    std::lock_guard<std::mutex> guard(state->mutex);
    state->looper.reset();
  }
  state->threadId.store(std::thread::id{}, std::memory_order_release);
  state->runFinished.store(true, std::memory_order_release);
}

bool EventThread::isAlive() const {
  const auto state = currentState();
  return state != nullptr &&
         state->threadId.load(std::memory_order_acquire) != std::thread::id{};
}

std::shared_ptr<EventLooper> EventThread::getLooper() {
  const auto state = currentState();
  if (state == nullptr) {
    return nullptr;
  }
  std::lock_guard<std::mutex> guard(state->mutex);
  return state->looper;
}

std::shared_ptr<EventThread::State> EventThread::currentState() const {
  std::lock_guard<std::mutex> guard(mThreadLock);
  return mState;
}
//----------------------------------------- EventLatch -----------------------------------------------------------------
namespace {

/** The largest count a 32-bit wait word can express. */
constexpr size_t kMaxCount = std::numeric_limits<uint32_t>::max();

/** Clamps a requested count into the range the wait word supports. */
uint32_t clampCount(const size_t count) noexcept {
  return static_cast<uint32_t>(std::min(count, kMaxCount));
}

} // namespace anonymous

EventThread::EventLatch::EventLatch(const size_t count) noexcept
    : m_initial_count(clampCount(count)),
      m_remaining_count(clampCount(count)) {
}

void EventThread::EventLatch::reset(const size_t count) noexcept {
  const uint32_t clamped = clampCount(count);
  m_remaining_count.store(clamped, std::memory_order_release);
  // Every waiter has to re-read the counter, so wake them all. A waiter whose
  // count is still non-zero simply sleeps again.
  BlockingWait::notifyAll(m_remaining_count);
}

void EventThread::EventLatch::await() noexcept {
  for (;;) {
    const uint32_t remaining = m_remaining_count.load(std::memory_order_acquire);
    if (remaining == 0) {
      return;
    }
    // Sleep on the counter itself: the kernel only sleeps while it still holds
    // `remaining`, so a latch that lands before the sleep is not missed.
    (void) BlockingWait::wait(m_remaining_count, remaining, BlockingWait::kWaitForever);
  }
}

void EventThread::EventLatch::latch() noexcept {
  uint32_t remaining = m_remaining_count.load(std::memory_order_acquire);
  while (remaining > 0) {
    // A compare-exchange rather than a fetch_sub, so the counter can never wrap
    // past zero when latch() is called more often than the initial count.
    if (m_remaining_count.compare_exchange_weak(remaining, remaining - 1, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      if (remaining - 1 == 0) {
        // Reaching zero is the only transition waiters care about.
        BlockingWait::notifyAll(m_remaining_count);
      }
      return;
    }
  }
}

size_t EventThread::EventLatch::getCount() const noexcept {
  const uint32_t remaining = m_remaining_count.load(std::memory_order_acquire);
  return static_cast<size_t>(m_initial_count - remaining);
}

} // namespace coral
