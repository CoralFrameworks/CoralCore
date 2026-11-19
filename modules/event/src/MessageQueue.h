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

#ifndef CORAL_EVENT_MESSAGE_QUEUE_H
#define CORAL_EVENT_MESSAGE_QUEUE_H

#include <core/event/Definitions.h>
#include <core/event/EventLooper.h>
#include <core/event/Message.h>

#include "BlockingWait.h"
#include "MessageBuffer.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace coral {

/**
 * Low-level class holding the list of messages to be dispatched by a {@link EventLooper}.
 *
 * Messages are not added directly to a MessageQueue, but rather through {@link EventHandler}
 * objects associated with the EventLooper.
 *
 * You can retrieve the MessageQueue for the current thread with {@link EventLooper::myQueue()}.
 *
 * @discussion
 * The queue is lock-free on the posting path. Producers and the looper thread
 * share no mutex and no critical section:
 *
 *  - two growable lanes move messages from any thread to the looper thread, one
 *    for immediate delivery and one for delayed. Each lane is a bounded wait-free
 *    ring with an overflow list behind it, so a burst that fills the ring is
 *    buffered instead of dropped;
 *  - `removeMessages()` and `removeAllMessages()` cancel a message in place by
 *    setting an atomic flag, and the looper skips cancelled messages when it
 *    drains a lane. There is no removal from the middle of a queue, so there is
 *    nothing to lock;
 *  - blocking is done by sleeping on an {@link EventCounter} sequence through the
 *    kernel's wait primitive, not on a condition variable.
 *
 * A delayed message is released back to the pending lane the moment its deadline
 * passes, so the sequence ordering between an immediate and a delayed message is
 * decided when both are already due.
 *
 * @section sync_barrier Synchronisation barriers
 *
 * A barrier holds back *synchronous* messages while letting *asynchronous* ones
 * through. It is the same mechanism Android uses to keep a VSYNC or an input
 * event from queueing behind a backlog of ordinary work: post the urgent work as
 * asynchronous, derive a barrier, and ordinary messages pile up until the barrier
 * is released and the frame is finished.
 *
 * Two things are worth knowing about how it behaves here:
 *
 *  - a barrier holds back the messages of the thread that took it too. That thread
 *    must therefore not wait on a synchronous message of its own while it holds
 *    one, because only it can release the barrier — post such work through the
 *    asynchronous send paths instead;
 *  - parking a message to let urgent work past it costs nothing but a move. The
 *    held messages keep their posting order and are handed back, ahead of anything
 *    posted after the release, as soon as the barrier is dropped.
 */
class MessageQueue final {
public:
  /**
   * The number of messages that can be outstanding in each wait-free ring.
   *
   * Delayed messages occupy a separate lane from immediate ones, so a burst of
   * scheduled work cannot starve ordinary posts. The ring is only the hot path:
   * past this many outstanding messages the lane spills into its overflow list
   * and keeps accepting, so this bounds the fast path rather than the queue.
   */
  static constexpr size_t kDefaultCapacity = 1024;

  /**
   * Holds a synchronisation barrier for as long as it is alive.
   *
   * Returned by `setSyncBarrier()`. It keeps the caller from having to pair the
   * calls by hand on every early return, which is exactly the mistake that turns
   * a barrier into a permanently stalled queue.
   */
  class BarrierGuard final {
  public:
    BarrierGuard(MessageQueue& queue, const bool active) noexcept
        : mQueue(&queue),
          mActive(active) {}

    ~BarrierGuard() {
      if (mActive) {
        mQueue->releaseSyncBarrier();
      }
    }

    BarrierGuard(const BarrierGuard&) = delete;
    BarrierGuard& operator=(const BarrierGuard&) = delete;

    BarrierGuard(BarrierGuard&& other) noexcept
        : mQueue(other.mQueue),
          mActive(other.mActive) {
      other.mActive = false;
    }

    BarrierGuard& operator=(BarrierGuard&&) = delete;

    /** Whether this guard actually holds a barrier. */
    [[nodiscard]] bool active() const noexcept {
      return mActive;
    }

    /** Releases the barrier early, if this guard still holds one. */
    void release() noexcept {
      if (mActive) {
        mQueue->releaseSyncBarrier();
        mActive = false;
      }
    }

  private:
    MessageQueue* mQueue;
    bool mActive;
  };

  explicit MessageQueue(bool quitAllowed, EventLooper& looper, size_t capacity = kDefaultCapacity) noexcept;

  ~MessageQueue();

  MessageQueue(const MessageQueue&) = delete;
  MessageQueue& operator=(const MessageQueue&) = delete;

  /**
   * Asks the looper thread to stop.
   *
   * @param safe when true, messages that are already due are delivered before the
   *             loop ends and future ones are dropped; when false, everything
   *             pending is dropped immediately.
   *
   * @discussion
   * This only sets an atomic flag and wakes the looper, so it is safe to call
   * from any thread, including a message handler running on the looper thread.
   */
  void quit(bool safe);

  /** Whether a message with this `what` is queued for the handler `handlerId`. */
  [[nodiscard]] bool hasMessages(const std::shared_ptr<EventHandler>& handler, int msgWhat) const;

  /** Whether a message with this `what` is queued for `handlerId`. */
  [[nodiscard]] bool hasMessagesFor(uint64_t handlerId, int msgWhat) const;

  /**
   * Adds a message, to be delivered at `targetTime` or later.
   *
   * @return `false` only when the looper is quitting, in which case the message
   *         is released. A full ring is not a failure: the message is buffered in
   *         the lane's overflow list and delivered in order.
   */
  bool enqueueMessage(MessagePtr message, TimePoint targetTime);

  /**
   * Cancels every queued message for `handler` with this `what`.
   *
   * Only what was already queued when the call started is withdrawn. A message
   * posted while this runs — from another thread, or from the looper thread — is
   * left alone, which is what makes "cancel the pending work, then post a
   * replacement" behave the way the caller means. Cancelling everything that
   * matches would instead swallow the replacement.
   */
  void removeMessages(const std::shared_ptr<EventHandler>& handler, int msgWhat);

  /**
   * Cancels every queued message for `handler`.
   *
   * Subject to the same "already queued" rule as {@link removeMessages}.
   */
  void removeAllMessages(const std::shared_ptr<EventHandler>& handler);

  /**
   * Cancels every queued message for `handler` with this `what`.
   *
   * Matched on the identity a message carries rather than on a pointer to its
   * handler, so this keeps working while the handler is being destroyed — the case
   * it exists for. See `EventHandler::id()`.
   */
  void removeMessagesFor(uint64_t handlerId, int msgWhat);

  /** Cancels every already-queued message for `handlerId`, in every lane. */
  void removeAllMessagesFor(uint64_t handlerId);

  /**
   * Holds back synchronous messages until `releaseSyncBarrier()` is called.
   *
   * Asynchronous messages keep being delivered, which is the whole point: the
   * barrier exists so urgent work can overtake ordinary work. Later barriers
   * stack over earlier ones and every one of them has to be released.
   *
   * @return `false` when a barrier is already held by another thread, in which
   *         case nothing changes. A barrier holds back the messages of the thread
   *         that took it as well, so that thread must post asynchronous work while
   *         it holds one rather than waiting on a synchronous message of its own.
   */
  bool setSyncBarrier();

  /**
   * Releases one level of the barrier this thread derived.
   *
   * @return `false` when this thread held no barrier, in which case nothing
   *         changes. Nobody else can release it.
   */
  bool releaseSyncBarrier();

  /**
   * Releases up to `levels` of the barrier this thread derived.
   *
   * This is for a handler that is being torn down while it still counts barrier
   * levels: every level it took has to be given back, or the queue stays
   * synchronised for good.
   *
   * @return how many levels were actually released.
   */
  int releaseSyncBarrier(int levels);

  /** Whether any synchronisation barrier is currently held. */
  [[nodiscard]] bool isSyncBarrierSet() const noexcept {
    return mSyncBarrier.load(std::memory_order_acquire);
  }

  /**
   * How many messages a barrier has held back so far.
   *
   * Monotonic, so a caller can watch it move rather than guess from a sleep. It
   * is the observable side of a barrier: a count that goes up while the barrier is
   * held, and stops going up once it is released.
   *
   * @see EventHandler::parkedByBarrierCount
   */
  [[nodiscard]] uint64_t parkedByBarrierCount() const noexcept {
    return mParkedByBarrier.load(std::memory_order_acquire);
  }

  /** Whether the queue is quitting. */
  [[nodiscard]] bool isQuitting() const noexcept {
    return mQuitting.load(std::memory_order_acquire);
  }

  /**
   * The reference point for a wait, to be captured before any state is inspected.
   *
   * Kept here rather than read inside `nextTask` because its ordering matters: a
   * change that lands between the state check and the wait would otherwise be
   * waited on rather than noticed, costing a full timeout of latency.
   */
  [[nodiscard]] uint32_t observeNotify() const noexcept {
    return mNotify.sequence();
  }

  /** One entry in a lane: the message plus what the looper needs to triage it. */
  struct PendingMessage {    /** The message itself. Null means the slot holds nothing. */
    Message* payload = nullptr;
    /** Posting order across all lanes, so ordering survives the split. */
    uint64_t order = 0;
    /** When the message becomes deliverable. */
    TimePoint targetTime{};

    /** Whether this entry still holds an undelivered message. */
    [[nodiscard]] bool isLive() const noexcept {
      return payload != nullptr && !payload->isCancelled();
    }
  };

private:
  struct NextTask {
    bool final_task_{ false };
    /** Something was handed back and only needs another look to be delivered. */
    bool ready_follow_up_{ false };
    MessagePtr run_msg_;
    TimeDelta sleep_time{};
  };

  /**
   * Collects the next deliverable message, waiting up to `timeout` for one.
   *
   * This replaces the old locked `getNextTask()` plus a separate `wait()` pair:
   * deciding to sleep and sleeping are one operation, so no wake-up can be lost
   * between them.
   *
   * @param observed a notification sequence read *before* this call inspected any
   *                 state, so a change that landed while it ran is noticed instead
   *                 of waited on.
   * @param timeout  how long to wait for work, or a negative value for ever.
   */
  NextTask nextTask(uint32_t observed, TimeDelta timeout);

  /** Moves delayed messages whose deadline has passed into the pending lane. */
  void releaseDueMessages();

  /**
   * Takes the next pending message that may be delivered right now.
   *
   * A synchronous message that a barrier is holding back is moved to the blocked
   * lane rather than delivered, so later messages can still get past it. The
   * returned pointer carries the pool's recycler, so a delivered message goes back
   * to the pool rather than to the allocator.
   */
  MessagePtr takePending();

  /** Whether `message` has to wait for the barrier currently being held. */
  [[nodiscard]] bool isBlockedByBarrier(const Message& message) const noexcept;

  /** The deadline of the earliest delayed message, if there is one. */
  [[nodiscard]] std::optional<TimePoint> earliestDelayedDeadline() const;

  /** Returns parked messages to the pending lane, oldest first. */
  void restoreBlockedMessages();

  /** Drains one lane, destroying every entry it holds. */
  void purgeBuffer(MessageBuffer<PendingMessage>& buffer);

  friend class EventLooper;

  EventLooper& looper;

  /** False for a queue that is not allowed to quit, such as a thread's own. */
  const bool mQuitAllowed;

  /** Set by `quit()`. Read on the posting path to reject new work. */
  std::atomic_bool mQuitting{ false };

  /** Bumped for every post, giving a total order across every lane. */
  std::atomic<uint64_t> mPostingOrder{ 0 };

  /** Bumped whenever the looper has something new to look at. */
  mutable EventCounter mNotify{};

  /** Set while a barrier holds back synchronous messages. */
  std::atomic_bool mSyncBarrier{ false };

  /** Which thread derived the barrier, so nobody else can release it. */
  std::atomic<std::thread::id> mBarrierOwner{};

  /** Guards `mBarrierOwner` and counts this thread's stacked barriers. */
  std::mutex mBarrierMutex;

  /** How many barriers the owning thread is holding. */
  int mBarrierDepth = 0;

  /** Bumped whenever the barrier is released, so the hand-back is not missed. */
  std::atomic<uint64_t> mBarrierReleaseCount{ 0 };

  /** Bumped for every message a barrier held back. */
  std::atomic<uint64_t> mParkedByBarrier{ 0 };

  /** Immediate messages, delivered in posting order. */
  MessageBuffer<PendingMessage> mPending;

  /** Delayed messages, moved to `mPending` once due. */
  MessageBuffer<PendingMessage> mDelayed;

  /**
   * Messages a barrier is holding back.
   *
   * Only the looper thread ever touches this lane: it parks a held message here
   * as it drains `mPending`, and it puts them back when the barrier goes away, so
   * no producer ever reaches it and no lock is needed for it.
   */
  MessageBuffer<PendingMessage> mBlocked;

  /** The release count the blocked lane was last emptied at. */
  uint64_t mDrainedAtRelease = 0;
};

} // namespace coral

#endif // CORAL_EVENT_MESSAGE_QUEUE_H
