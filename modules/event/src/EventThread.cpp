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
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <time.h>
#elif defined(WIN32)
#define NOMINMAX
#include <windows.h>
#include <avrt.h>   // MMCSS
#pragma comment(lib, "avrt.lib")
#endif

namespace coral {

namespace {

void setThreadName(char const* name) noexcept {
#if defined(__linux__)
  pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
  pthread_setname_np(name);
#elif defined(WIN32)
  std::string_view u8name(name);
  size_t size = MultiByteToWideChar(CP_UTF8, 0, u8name.data(), u8name.size(), nullptr, 0);

  std::wstring u16name;
  u16name.resize(size);
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8name.data(), u8name.size(), u16name.data(), u16name.size());

  SetThreadDescription(GetCurrentThread(), u16name.data());
#endif
}

} // namespace anonymous

EventThread::EventThread(std::string name) noexcept
    : threadName(std::move(name)),
      barrier(1) {
}

EventThread::~EventThread() {
  // A joinable std::thread destroyed without join or detach calls
  // std::terminate, and a looper thread left running would keep touching this
  // object after it is gone. Shut it down here so destruction is always safe.
  quit();
}

void EventThread::start() {
  {
    std::lock_guard<std::mutex> guard(mStartLock);
    if (mThread != nullptr) {
      return;
    }
    // Arm the handshake before the thread runs. The latch is one-shot, so it has
    // to be re-armed for a restart; nothing is waiting in it at this point, which
    // is what makes the reset safe.
    barrier.reset(1);
    // The handle is installed before the thread runs, so nothing else has to be
    // published for the stop paths to find it.
    mThread = std::make_unique<std::thread>(&EventThread::run, this);
  }

  // Wait until the looper exists. The barrier is the handshake: `run()` signals it
  // once the looper and the thread id are published, so a caller can use the
  // looper and see the thread as alive immediately after start() returns.
  //
  // The wait happens outside `mStartLock` on purpose: `run()` takes that lock to
  // publish the looper, so holding it here while waiting would deadlock.
  barrier.await();
}

void EventThread::releaseThread(const bool join) {
  // The handle is taken under the lock and then released, so joining happens
  // without holding it. This matters: `run()` takes the same lock on its way out,
  // so joining while holding it would deadlock.
  std::unique_ptr<std::thread> handle;
  {
    std::lock_guard<std::mutex> guard(mStartLock);
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
    // Either a detach was asked for, or this is the looper thread itself, where
    // joining would deadlock. Letting it finish on its own is the only option.
    handle->detach();
  }

  // The handle is gone, so this object no longer considers the thread alive. This
  // also has to clear the id: when the thread was detached, it may not have run
  // its own cleanup yet, and a stale id would make isAlive() lie and block a
  // restart.
  mThreadId.store(std::thread::id{}, std::memory_order_release);
}

bool EventThread::quit() {
  const auto looper = getLooper();
  if (looper == nullptr) {
    // Never started, or already gone.
    releaseThread(true);
    return false;
  }

  looper->quit();
  // Joining is only safe from another thread: when quit() is called from a
  // handler running on the looper thread, joining would deadlock.
  releaseThread(!looper->isCurrentThread());
  return true;
}

bool EventThread::quitSafety() {
  const auto looper = getLooper();
  if (looper == nullptr) {
    releaseThread(true);
    return false;
  }

  looper->quitSafety();
  releaseThread(!looper->isCurrentThread());
  return true;
}

void EventThread::run() {
  if (!threadName.empty()) {
    setThreadName(threadName.c_str());
  }

  EventLooper::prepare();
  MINILOG_DEBUG("The event thread '{}' prepared its looper", threadName);
  {
    // Publish the looper and the id together, under the lock, so whoever start()
    // is waiting for sees a complete picture: a thread that is alive and has a
    // usable looper. The latch is fired only after this, which is what makes
    // start() return with both already visible.
    std::lock_guard<std::mutex> guard(mStartLock);
    mLooper = EventLooper::myLooper();
    mThreadId.store(std::this_thread::get_id(), std::memory_order_release);
  }
  barrier.latch();

  EventLooper::loop();

  {
    const auto id = mThreadId.load(std::memory_order_acquire);
    if (id != std::thread::id{}) {
      EventLooper::remove(id);
    }
  }
  {
    std::lock_guard<std::mutex> guard(mStartLock);
    mLooper.reset();
  }
  mThreadId.store(std::thread::id{}, std::memory_order_release);
}

bool EventThread::isAlive() const {
  return mThreadId.load(std::memory_order_acquire) != std::thread::id{};
}

const std::shared_ptr<EventLooper>& EventThread::getLooper() {
  static const std::shared_ptr<EventLooper> sEmptyLooper;

  // The looper is handed out from under the same lock that publishes it, so a
  // reader either sees nothing yet or a complete object; there is no polling loop
  // and no read of a pointer that another thread is writing.
  {
    std::lock_guard<std::mutex> guard(mStartLock);
    if (mLooper != nullptr) {
      // Returned by reference, so the caller's binding stays valid as long as the
      // thread object does; `mLooper` is only cleared by `run()` before the
      // thread exits and by the destructor.
      return mLooper;
    }
  }

  return sEmptyLooper;
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
    BlockingWait::wait(m_remaining_count, remaining, BlockingWait::kWaitForever);
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
