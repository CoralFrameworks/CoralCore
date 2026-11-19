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

#include "MessageQueue.h"

#include <utility>

namespace coral {

namespace {

/**
 * Hands out handler identities.
 *
 * Only equality is ever used, so a counter is enough and never repeats within a
 * process.
 */
std::atomic<uint64_t>& nextHandlerId() noexcept {
  static std::atomic<uint64_t> next{ 0 };
  return next;
}

} // namespace anonymous

void EventHandler::BarrierGuard::release() noexcept {
  if (!mActive) {
    return;
  }
  mActive = false;
  const auto handler = std::move(mHandler);
  if (handler == nullptr) {
    return;
  }

  // The handler owns the token, and the queue holds the barrier until the count
  // reaches zero: a guard that is not the last one only gives up its own level, so
  // a nested barrier is not released early by an inner guard going out of scope.
  // Only the thread that takes the count from one to zero releases the queue.
  int depth = handler->barrierDepth.load(std::memory_order_acquire);
  while (depth > 0) {
    if (handler->barrierDepth.compare_exchange_weak(depth, depth - 1, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      if (depth == 1) {
        if (const auto queue = handler->msgQueue.lock()) {
          queue->releaseSyncBarrier();
        }
      }
      return;
    }
    // `depth` was refreshed by the failed exchange, so it is re-tested above.
  }
}

std::shared_ptr<EventHandler>
EventHandler::create(const std::shared_ptr<EventLooper>& looper, const std::shared_ptr<Callback>& callback) {
  return std::make_shared<EventHandler>(looper, callback);
}

EventHandler::EventHandler(const std::shared_ptr<EventLooper>& looper, const std::shared_ptr<Callback>& callback) noexcept
    : callback(callback),
      id_(nextHandlerId().fetch_add(1, std::memory_order_relaxed) + 1),
      looper(looper),
      msgQueue(looper ? looper->msgQueue : nullptr) {
}

EventHandler::~EventHandler() {
  // Withdraw anything this handler still has queued. The queue owns the only
  // owning reference to a posted message and drops a withdrawn message when it
  // drains it, so the handler can go away without leaving work behind that would
  // be dispatched to a dead callback.
  //
  // The match is done on the handler's identity rather than on a pointer to it:
  // inside a destructor the internal weak reference is already expired, so a
  // lookup through it would never match and nothing would be withdrawn.
  if (const auto queue = msgQueue.lock()) {
    queue->removeAllMessagesFor(id_);
    // A barrier this handler is still holding would otherwise hold back every
    // synchronous message on the queue for good. Every level has to be given back,
    // because the queue counts them.
    const int held = barrierDepth.exchange(0, std::memory_order_acq_rel);
    if (held > 0) {
      queue->releaseSyncBarrier(held);
    }
  }
}

void EventHandler::setCallback(const std::shared_ptr<Callback>& newCallback) {
  // The handler only ever holds the callback weakly, so replacing one cannot release
  // an owner's reference and nothing has to be destroyed outside the lock.
  const std::lock_guard<std::mutex> guard(callbackMutex);
  callback = newCallback;
}

bool EventHandler::hasCallback() const noexcept {
  const std::lock_guard<std::mutex> guard(callbackMutex);
  return !callback.expired();
}

bool EventHandler::hasMessages(const int what) {
  const auto queue = queueIfUsable();
  if (!queue) {
    return false;
  }
  return queue->hasMessagesFor(id_, what);
}

bool EventHandler::sendMessage(MessagePtr msg) {
  return sendMessageAtTime(std::move(msg), std::chrono::steady_clock::now());
}

bool EventHandler::sendMessageDelayed(MessagePtr msg, TimeDelta delay) {
  if (delay < TimeDelta::zero()) {
    delay = TimeDelta::zero();
  }
  return sendMessageAtTime(std::move(msg), std::chrono::steady_clock::now() + delay);
}

bool EventHandler::sendMessageAtTime(MessagePtr msg, const TimePoint targetTime,
    const bool async) {
  if (msg == nullptr) {
    return false;
  }
  const auto queue = queueIfUsable();
  if (!queue) {
    return false;
  }
  // Bind the target before the message becomes visible to the looper, so it can
  // never dispatch a message with a null target. The identity goes with it, so a
  // handler being destroyed can still withdraw its own messages, and the async
  // flag is set here for the same reason: the looper must see the finished
  // message.
  msg->target = getPtr();
  msg->targetId = id_;
  msg->setAsynchronous(async);
  return queue->enqueueMessage(std::move(msg), targetTime);
}

bool EventHandler::sendEmptyMessage(const int what) {
  return sendMessage(Message::obtain(getPtr(), what));
}

bool EventHandler::sendEmptyMessageDelayed(const int what, const TimeDelta delay) {
  return sendMessageDelayed(Message::obtain(getPtr(), what), delay);
}

bool EventHandler::sendEmptyMessageAtTime(const int what, const TimePoint targetTime) {
  return sendMessageAtTime(Message::obtain(getPtr(), what), targetTime);
}

bool EventHandler::sendEmptyMessageAsync(const int what) {
  return sendEmptyMessageAtTimeAsync(what, std::chrono::steady_clock::now());
}

bool EventHandler::sendEmptyMessageAsyncDelayed(const int what, const TimeDelta delay) {
  const TimeDelta wait =
      delay < TimeDelta::zero() ? TimeDelta::zero() : delay;
  return sendEmptyMessageAtTimeAsync(what, std::chrono::steady_clock::now() + wait);
}

bool EventHandler::sendEmptyMessageAtTimeAsync(const int what, const TimePoint targetTime) {
  return sendMessageAtTime(Message::obtain(getPtr(), what), targetTime, true);
}

void EventHandler::removeMessages(const int what) {
  const auto queue = queueIfUsable();
  if (!queue) {
    return;
  }
  queue->removeMessagesFor(id_, what);
}

void EventHandler::removeAllCallbacksAndMessages() {
  const auto queue = queueIfUsable();
  if (!queue) {
    return;
  }
  queue->removeAllMessagesFor(id_);
}

EventHandler::BarrierGuard EventHandler::deriveSyncBarrier() {
  const auto queue = queueIfUsable();
  if (!queue) {
    return BarrierGuard{};
  }
  if (!queue->setSyncBarrier()) {
    // Another thread is already synchronising this queue. A barrier is owned by
    // the thread that took it, so a second thread is refused rather than given a
    // level it could not release.
    return BarrierGuard{};
  }
  // Every successful derive is counted, including nested ones on this thread, so
  // the last guard to go out of scope is the one that releases the queue.
  barrierDepth.fetch_add(1, std::memory_order_acq_rel);
  return BarrierGuard{ getPtr(), true };
}

bool EventHandler::isSyncBarrierSet() const {
  const auto queue = msgQueue.lock();
  return queue != nullptr && queue->isSyncBarrierSet();
}

uint64_t EventHandler::parkedByBarrierCount() const {
  const auto queue = msgQueue.lock();
  return queue != nullptr ? queue->parkedByBarrierCount() : 0;
}

bool EventHandler::postTask(std::unique_ptr<QueuedTask> task) {
  return sendMessage(Message::obtain(getPtr(), std::move(task)));
}

bool EventHandler::postTaskDelay(std::unique_ptr<QueuedTask> task, const TimeDelta delay) {
  return sendMessageDelayed(Message::obtain(getPtr(), std::move(task)), delay);
}

bool EventHandler::postTaskAtTime(std::unique_ptr<QueuedTask> task,
    const TimePoint targetTime) {
  return sendMessageAtTime(Message::obtain(getPtr(), std::move(task)), targetTime);
}

std::shared_ptr<MessageQueue> EventHandler::queueIfUsable() const {
  // Both locks are needed: the looper may have been destroyed (nothing can be
  // delivered any more) and the queue may have been asked to quit. Checking the
  // queue through the shared pointer rather than a raw reference keeps the check
  // free of a use-after-free window.
  auto queue = msgQueue.lock();
  if (!queue || queue->isQuitting()) {
    return nullptr;
  }
  return queue;
}

void EventHandler::dispatchMessage(Message* msg) {
  if (msg == nullptr) {
    return;
  }
  if (msg->run_task_) {
    msg->run_task_->run();
    msg->run_task_.reset();
    return;
  }
  // The callback reference is read under its lock, because `setCallback()` can
  // replace it from another thread. The lock is held only long enough to promote
  // the weak reference: the call itself runs unlocked, so a callback that posts
  // back into this handler cannot deadlock on it.
  //
  // Promoting it to a strong reference is what makes a destroyed callback safe. A
  // raw pointer would be read here long after the object it named was gone, and
  // the call below would land in freed memory; an expired weak reference instead
  // reports that there is nothing to call, and the message falls through to
  // `handleMessage()`.
  std::shared_ptr<Callback> target;
  {
    const std::lock_guard<std::mutex> guard(callbackMutex);
    target = callback.lock();
  }
  if (target != nullptr && target->handleMessage(msg)) {
    return;
  }
  handleMessage(msg);
}

} // namespace coral
