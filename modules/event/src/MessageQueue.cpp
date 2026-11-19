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

#include "MessageQueue.h"

#include <core/event/EventHandler.h>

#include <algorithm>
#include <functional>
#include <thread>

namespace coral {

namespace {

/**
 * Destroys a lane entry's payload.
 *
 * A posted message is owned by the queue until the looper drops it, so this is
 * the only place a queued message is destroyed. Destroying the owning pointer
 * also returns the message to the pool it came from, when it came from one.
 */
void releaseEntry(MessageQueue::PendingMessage& entry) {
  delete entry.payload;
  entry.payload = nullptr;
}

} // namespace anonymous

MessageQueue::MessageQueue(const bool quitAllowed, EventLooper& looper, const size_t capacity) noexcept
    : looper(looper),
      mQuitAllowed(quitAllowed),
      mPending(capacity),
      mDelayed(capacity),
      mBlocked(capacity) {
}

MessageQueue::~MessageQueue() {
  purgeBuffer(mPending);
  purgeBuffer(mDelayed);
  purgeBuffer(mBlocked);
}

void MessageQueue::purgeBuffer(MessageBuffer<PendingMessage>& buffer) {
  PendingMessage entry{};
  while (buffer.pop(entry)) {
    releaseEntry(entry);
  }
}

void MessageQueue::quit(const bool safe) {
  if (!mQuitAllowed) {
    return;
  }
  // A one-time transition, so a repeated or concurrent quit is harmless. This is
  // a single atomic operation, so quit() is safe to call from a message handler
  // running on the looper thread itself.
  bool expected = false;
  if (!mQuitting.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    return;
  }
  if (!safe) {
    // Ask the looper to drop the future work. The delayed lane is *not* purged
    // here: the lane has a single consumer, and draining it from this thread while
    // the looper is inside `releaseDueMessages()` would let both of them pop the
    // same entry and destroy the same message twice. The flag is enough — the
    // looper empties both lanes on its way out.
    mQuitting.store(true, std::memory_order_release);
  }
  // Wake the looper so it re-evaluates and sees the quit.
  mNotify.signalAll();
}

bool MessageQueue::setSyncBarrier() {
  const std::lock_guard<std::mutex> guard(mBarrierMutex);
  const auto self = std::this_thread::get_id();

  if (mSyncBarrier.load(std::memory_order_acquire)) {
    if (mBarrierOwner.load(std::memory_order_acquire) != self) {
      // Someone else is already synchronising. Silently ignoring this request is
      // what keeps a second caller from releasing a barrier it does not own.
      return false;
    }
    ++mBarrierDepth;
    return true;
  }

  mBarrierDepth = 1;
  mBarrierOwner.store(self, std::memory_order_release);
  mSyncBarrier.store(true, std::memory_order_release);

  // Wake the looper: a synchronous message it is about to dispatch may now be
  // held back, and it has to notice before it hands it over.
  mNotify.signal();
  return true;
}

bool MessageQueue::releaseSyncBarrier() {
  return releaseSyncBarrier(1) == 1;
}

int MessageQueue::releaseSyncBarrier(const int levels) {
  int released = 0;
  {
    const std::lock_guard<std::mutex> guard(mBarrierMutex);
    if (!mSyncBarrier.load(std::memory_order_acquire) ||
        mBarrierOwner.load(std::memory_order_acquire) != std::this_thread::get_id()) {
      // Not ours to release, or nothing is held.
      return 0;
    }
    while (released < levels && mBarrierDepth > 0) {
      --mBarrierDepth;
      ++released;
    }
    if (released == 0 || mBarrierDepth > 0) {
      // A nested barrier is still held, so the queue stays synchronised.
      return released;
    }

    mBarrierOwner.store(std::thread::id{}, std::memory_order_release);
    mSyncBarrier.store(false, std::memory_order_release);
    mBarrierReleaseCount.fetch_add(1, std::memory_order_acq_rel);
  }

  // Waking the looper is what hands the held messages back. It is done outside
  // the lock because the looper will want the lock itself when it drains the
  // blocked lane.
  mNotify.signalAll();
  return released;
}

bool MessageQueue::hasMessages(const std::shared_ptr<EventHandler>& handler, const int msgWhat) const {
  if (!handler) {
    return false;
  }
  return hasMessagesFor(handler->id(), msgWhat);
}

bool MessageQueue::hasMessagesFor(const uint64_t handlerId, const int msgWhat) const {
  // The lanes are read in place, without draining and reposting them, so a
  // concurrent post cannot be reordered by this query. Only the payload pointer
  // and an atomic flag are read, so no producer lock is needed to inspect a live
  // entry. The blocked lane is owned by the looper thread; reading it from here is
  // a best-effort hint, exactly like the rest of this query.
  const auto matches = [&](const PendingMessage& entry) {
    return entry.isLive() && entry.payload->targetId == handlerId && entry.payload->what == msgWhat;
  };

  bool found = false;
  mPending.tryVisit([&](const PendingMessage& entry) {
    if (!found && matches(entry)) {
      found = true;
    }
  });
  if (found) {
    return true;
  }
  mDelayed.tryVisit([&](const PendingMessage& entry) {
    if (!found && matches(entry)) {
      found = true;
    }
  });
  if (found) {
    return true;
  }
  mBlocked.tryVisit([&](const PendingMessage& entry) {
    if (!found && matches(entry)) {
      found = true;
    }
  });
  return found;
}

bool MessageQueue::enqueueMessage(MessagePtr message, const TimePoint targetTime) {
  if (message == nullptr || mQuitting.load(std::memory_order_acquire)) {
    return false;
  }

  PendingMessage entry{};
  entry.payload = message.release();
  entry.order = mPostingOrder.fetch_add(1, std::memory_order_relaxed) + 1;
  entry.targetTime = targetTime;

  // A message whose deadline has not passed goes into the delayed lane and is
  // promoted to the pending lane once due. Every lane is drained by the same
  // thread, so the posting order carried in `order` survives the split.
  if (targetTime <= std::chrono::steady_clock::now()) {
    mPending.push(entry);
  } else {
    mDelayed.push(entry);
  }

  // Published after the push, so a waiter that read the old sequence re-checks
  // and finds this message instead of sleeping through it.
  mNotify.signal();
  return true;
}

void MessageQueue::removeMessages(const std::shared_ptr<EventHandler>& handler, const int msgWhat) {
  if (!handler) {
    return;
  }
  removeMessagesFor(handler->id(), msgWhat);
}

void MessageQueue::removeMessagesFor(const uint64_t handlerId, const int msgWhat) {
  // Only what was already queued when this call started is withdrawn. A message
  // posted while this runs — from another thread, or from the looper thread itself
  // — survives, which is what makes `removeMessages()` followed by a fresh post of
  // the same code work. Walking the queue and cancelling everything that matches
  // would instead swallow the replacement, and that replacement is usually posted
  // precisely because the old work was just cancelled.
  //
  // The watermark is the posting order this call observes: everything already
  // queued was stamped below it, and everything posted afterwards is stamped above
  // it.
  const uint64_t watermark = mPostingOrder.load(std::memory_order_acquire);

  const auto withdraw = [&](const PendingMessage& entry) {
    if (!entry.isLive() || entry.order > watermark) {
      return;
    }
    if (entry.payload->targetId == handlerId && entry.payload->what == msgWhat) {
      entry.payload->cancel();
    }
  };
  mPending.tryVisit(withdraw);
  mDelayed.tryVisit(withdraw);
  // A message a barrier is holding back is still queued, so it has to be
  // withdrawable too; otherwise it would be reported as pending and then delivered.
  mBlocked.tryVisit(withdraw);

  // A withdrawn delayed message can change how long the looper should sleep, so
  // prod it to re-evaluate.
  mNotify.signal();
}

void MessageQueue::removeAllMessages(const std::shared_ptr<EventHandler>& handler) {
  if (!handler) {
    return;
  }
  removeAllMessagesFor(handler->id());
}

void MessageQueue::removeAllMessagesFor(const uint64_t handlerId) {
  // Same watermark rule as `removeMessagesFor`, for the same reason: purging
  // everything for a handler must not reach work that arrived after the purge
  // began.
  const uint64_t watermark = mPostingOrder.load(std::memory_order_acquire);

  const auto withdraw = [&](const PendingMessage& entry) {
    if (entry.isLive() && entry.order <= watermark && entry.payload->targetId == handlerId) {
      entry.payload->cancel();
    }
  };
  mPending.tryVisit(withdraw);
  mDelayed.tryVisit(withdraw);
  mBlocked.tryVisit(withdraw);
  mNotify.signal();
}

std::optional<TimePoint> MessageQueue::earliestDelayedDeadline() const {
  std::optional<TimePoint> earliest;
  mDelayed.tryVisit([&](const PendingMessage& entry) {
    if (!entry.isLive()) {
      return;
    }
    if (!earliest.has_value() || entry.targetTime < *earliest) {
      earliest = entry.targetTime;
    }
  });
  return earliest;
}

void MessageQueue::releaseDueMessages() {
  PendingMessage entry{};
  while (mDelayed.pop(entry)) {
    if (!entry.isLive()) {
      // Withdrawn while it waited: drop it rather than deliver it.
      releaseEntry(entry);
      continue;
    }
    if (entry.targetTime > std::chrono::steady_clock::now()) {
      // Not due yet. The lane is drained in posting order, so once one entry is
      // still in the future nothing behind it can be due either: put it back and
      // stop, leaving the rest for the next look. The lane grows rather than
      // drops, so this re-post cannot fail.
      mDelayed.push(entry);
      break;
    }

    // Due now: hand it to the pending lane so both kinds of message are delivered
    // through one ordered path.
    mPending.push(entry);
  }
}

bool MessageQueue::isBlockedByBarrier(const Message& message) const noexcept {
  if (!mSyncBarrier.load(std::memory_order_acquire)) {
    return false;
  }
  // An asynchronous message is exactly the kind that has to get past the barrier,
  // which is how a VSYNC or an input event reaches the frame being set up.
  return !message.isAsynchronous();
}

MessagePtr MessageQueue::takePending() {
  PendingMessage entry{};
  while (mPending.pop(entry)) {
    if (entry.payload == nullptr) {
      continue;
    }
    if (entry.payload->isCancelled()) {
      // Withdrawn before delivery: destroy it here, on the looper thread, which
      // is the only thread allowed to destroy a queued message.
      releaseEntry(entry);
      continue;
    }
    if (entry.payload->isAsynchronous() || !isBlockedByBarrier(*entry.payload)) {
      // Reconstructed as a pooled pointer, not a plain one: the deleter is what
      // returns the message to the pool, and the pool is the whole reason a short
      // message does not cost an allocation and a free.
      return MessagePtr{ entry.payload, MessageRecycler{ MessagePool::global() } };
    }

    // A barrier is up and this message has to wait. Park it instead of dropping it
    // or holding it in place: a message left at the head of the lane would stop
    // every asynchronous message behind it, and the barrier would then be blocking
    // the very work it exists to let through.
    mParkedByBarrier.fetch_add(1, std::memory_order_acq_rel);
    mBlocked.push(entry);
  }
  return nullptr;
}

void MessageQueue::restoreBlockedMessages() {
  const uint64_t released = mBarrierReleaseCount.load(std::memory_order_acquire);
  const bool quitting = mQuitting.load(std::memory_order_acquire);
  if (released == mDrainedAtRelease && !quitting) {
    return;
  }
  // Recorded before draining, so a release that lands while this runs is noticed
  // by the next look: the count will have moved on again and the lane is drained
  // once more. Draining twice is harmless; missing a drain would strand the
  // messages the barrier was holding.
  mDrainedAtRelease = released;

  PendingMessage entry{};
  while (mBlocked.pop(entry)) {
    // Pushed to the front rather than the back: these were posted before anything
    // a producer can added since the barrier was released, so they have to be
    // delivered first. The order stamps keep them in order among themselves.
    mPending.pushFront(entry);
  }
}

MessageQueue::NextTask MessageQueue::nextTask(const uint32_t observed, const TimeDelta timeout) {
  NextTask result{};

  // Promote anything whose deadline has passed before looking for work.
  releaseDueMessages();
  // Hand back what a barrier was holding. A quit does this too, so a safe quit
  // delivers work that was only waiting for a barrier rather than dropping it with
  // the future work.
  restoreBlockedMessages();

  if (auto message = takePending()) {
    result.run_msg_ = std::move(message);
    return result;
  }

  // Everything that was due may now be sitting behind a re-raised barrier, so give
  // it another look before deciding the queue is idle.
  if (isSyncBarrierSet() && mBarrierReleaseCount.load(std::memory_order_acquire) != mDrainedAtRelease) {
    result.ready_follow_up_ = true;
    return result;
  }

  // Nothing to do. A quit only ends the loop once the due work has been drained,
  // because `takePending()` above would have returned it.
  if (mQuitting.load(std::memory_order_acquire)) {
    result.final_task_ = true;
    return result;
  }

  // Work out how long to sleep. `observed` was read by the caller before this
  // inspected any state, which is what makes the wake-up reliable: a post or a
  // barrier release that landed since then has already bumped the sequence, so the
  // wait below returns immediately instead of sleeping through it.
  TimeDelta sleepFor = timeout;
  if (const auto deadline = earliestDelayedDeadline()) {
    const auto untilDue =
        std::chrono::duration_cast<TimeDelta>(*deadline - std::chrono::steady_clock::now());
    if (untilDue <= TimeDelta::zero()) {
      // Something became due while this ran: loop again instead of sleeping.
      result.sleep_time = TimeDelta::zero();
      return result;
    }
    sleepFor = timeout < TimeDelta::zero() ? untilDue : std::min(timeout, untilDue);
  }

  if (sleepFor <= TimeDelta::zero()) {
    // A zero timeout means "do not block", so report no work and let the caller
    // decide whether to spin or to come back later.
    return result;
  }

  (void) mNotify.waitForChange(observed, sleepFor.count());
  return result;
}

} // namespace coral
