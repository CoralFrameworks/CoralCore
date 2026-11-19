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

#include <core/event/EventHandler.h>
#include <core/event/EventLooper.h>

#include <minilog/log.h>

#include "MessageQueue.h"

#include <chrono>
#include <map>
#include <utility>

namespace coral {

namespace {

/**
 * A time a looper is willing to sleep for when it has nothing to do.
 *
 * The looper blocks on the queue's notification instead of polling, so this is
 * only a backstop against a platform without a kernel wait: it keeps the loop
 * from sleeping forever if a wake-up is ever lost on such a build.
 */
constexpr TimeDelta kMaxIdleSleep = std::chrono::milliseconds(1000);

} // namespace anonymous

/**
 * The looper registry for one thread.
 *
 * Every looper belongs to the thread that created it and is only ever reached
 * through that thread's registry, so the map needs no lock: a `Holder` is
 * thread-local, and the id used as its key is always the owning thread's id.
 *
 * The registry deliberately holds the looper rather than a raw pointer, so the
 * looper (and its queue) stays alive as long as a handler can still reach it.
 */
class EventLooper::Holder {
public:
  /** The looper for `id`, or null when this thread has none. */
  [[nodiscard]] const std::shared_ptr<EventLooper>& getLooper(const std::thread::id& id) const noexcept {
    const auto found = sLooperHolder.find(id);
    if (found == sLooperHolder.end()) {
      return sEmptyLooper;
    }
    return found->second;
  }

  void prepareLooper(const std::thread::id& id) {
    if (sLooperHolder.find(id) != sLooperHolder.end()) {
      MINILOG_ERROR("Only one Looper may be created per thread");
      return;
    }
    sLooperHolder.emplace(id, std::shared_ptr<EventLooper>(new EventLooper(id, true)));
  }

  void removeLooper(const std::thread::id& id) noexcept {
    sLooperHolder.erase(id);
  }

private:
  static const std::shared_ptr<EventLooper> sEmptyLooper;
  std::map<std::thread::id, std::shared_ptr<EventLooper>> sLooperHolder;
};

const std::shared_ptr<EventLooper> EventLooper::Holder::sEmptyLooper = nullptr;

EventLooper::Holder& EventLooper::get() noexcept {
  static thread_local Holder instance;
  return instance;
}

EventLooper::EventLooper(std::thread::id id, bool quitAllowed) noexcept
    : threadId(id) {
  msgQueue = std::make_shared<MessageQueue>(quitAllowed, *this);
}

EventLooper::~EventLooper() = default;

void EventLooper::prepare() {
  get().prepareLooper(std::this_thread::get_id());
}

void EventLooper::loop() {
  const auto& looper = myLooper();
  if (looper == nullptr) {
    MINILOG_ERROR("The looper of this thread does not exist: the loop returns immediately");
    return;
  }
  const auto& queue = looper->msgQueue;
  MINILOG_DEBUG("The looper entered its loop");

  for (;;) {
    // The reference point is taken before `nextTask` inspects any state, so a post
    // or a barrier release that lands while it runs is noticed and wakes the wait
    // instead of being slept through.
    const uint32_t observed = queue->observeNotify();
    // `nextTask` also does the waiting, so there is no window between deciding to
    // sleep and sleeping in which a post could be missed.
    auto task = queue->nextTask(observed, kMaxIdleSleep);
    if (task.final_task_) {
      MINILOG_DEBUG("The looper left its loop because the queue quit");
      break;
    }
    if (task.ready_follow_up_) {
      // Something was handed back and only needs another look: do not sleep with
      // deliverable work sitting in the queue.
      continue;
    }
    if (!task.run_msg_) {
      // Nothing to do; `nextTask` already slept.
      continue;
    }

    auto msg = std::move(task.run_msg_);
    if (const auto handler = msg->target.lock()) {
      handler->dispatchMessage(msg.get());
    } else if (msg->run_task_) {
      // The handler is gone, but a closure can still be run: it was posted by
      // someone who wanted it executed on this thread regardless.
      if (msg->run_task_->run()) {
        msg->run_task_.reset();
      }
    }
  }
}

const std::shared_ptr<EventLooper>& EventLooper::myLooper() {
  const auto threadId = std::this_thread::get_id();
  return get().getLooper(threadId);
}

const std::shared_ptr<MessageQueue>& EventLooper::myQueue() {
  const auto& looper = myLooper();
  if (looper != nullptr) {
    return looper->msgQueue;
  }
  static const std::shared_ptr<MessageQueue> sEmptyQueue = nullptr;
  return sEmptyQueue;
}

bool EventLooper::hasLooper() noexcept {
  return myLooper() != nullptr;
}

bool EventLooper::isSyncBarrierSet() noexcept {
  const auto& queue = myQueue();
  return queue != nullptr && queue->isSyncBarrierSet();
}

void EventLooper::quit() { msgQueue->quit(false); }

void EventLooper::quitSafety() { msgQueue->quit(true); }

bool EventLooper::isCurrentThread() const {
  return threadId == std::this_thread::get_id();
}

void EventLooper::remove(const std::thread::id& id) {
  get().removeLooper(id);
}

} // namespace coral
