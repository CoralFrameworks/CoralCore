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

#ifndef CORAL_EVENT_EVENT_HANDLER_H
#define CORAL_EVENT_EVENT_HANDLER_H

#include <core/event/Definitions.h>
#include <core/event/Message.h>
#include <core/event/QueuedTask.h>

#include <atomic>
#include <cstddef>
#include <memory>

namespace coral {

class EventLooper;

class MessageQueue;

/**
 * A Handler allows you to send and process {@link Message} and QueuedTask
 * objects associated with a thread's {@link MessageQueue}.
 */
class EventHandler : public std::enable_shared_from_this<EventHandler> {
public:
  /**
   * Holds a synchronisation barrier for as long as it is alive.
   *
   * Returned by {@link deriveSyncBarrier}. It keeps the caller from having to
   * pair the set and release calls by hand on every early return, which is the
   * mistake that turns a barrier into a permanently stalled queue: a barrier that
   * is never released holds back every synchronous message for good.
   */
  class BarrierGuard final {
  public:
    BarrierGuard() noexcept = default;

    BarrierGuard(const std::shared_ptr<EventHandler>& handler, const bool active) noexcept
        : mHandler(handler),
          mActive(active) {}

    ~BarrierGuard() {
      release();
    }

    BarrierGuard(const BarrierGuard&) = delete;
    BarrierGuard& operator=(const BarrierGuard&) = delete;

    BarrierGuard(BarrierGuard&& other) noexcept
        : mHandler(std::move(other.mHandler)),
          mActive(other.mActive) {
      other.mActive = false;
    }

    BarrierGuard& operator=(BarrierGuard&&) = delete;

    /** Whether this guard actually holds a barrier. */
    [[nodiscard]] bool active() const noexcept {
      return mActive;
    }

    /** Releases the barrier early, if this guard still holds one. */
    void release() noexcept;

  private:
    /** Kept alive so the release path can still reach the queue. */
    std::shared_ptr<EventHandler> mHandler;
    bool mActive = false;
  };

  /**
   * Callback interface you can use when instantiating a Handler to avoid having
   * to implement your own subclass of Handler
   *
   * @section lifetime Lifetime
   *
   * The handler does **not** own the callback and must not outlive it. The callback
   * is held through a `weak_ptr`, so the caller is what keeps it alive, and the
   * handler can tell that it has been destroyed and simply stops delivering messages
   * to it:
   *
   * @code
   *   auto callback = std::make_shared<MyCallback>();
   *   auto handler = EventHandler::create(looper, callback);
   *   callback.reset();   // the handler keeps running, the dead callback is skipped
   * @endcode
   *
   * @warning A handler cannot itself be its own callback: `EventHandler` and
   * `Callback` both declare `handleMessage` with different return types, so a class
   * cannot derive from both. Use a separate callback object, or a subclass'
   * `handleMessage` override.
   */
  class Callback {
  public:
    virtual ~Callback() = default;

    /**
     * Called when we should handle message
     *
     * @param msg the message to handle. It is borrowed: the queue owns it and
     *            recycles it once the dispatch returns, so the callback must not
     *            keep the pointer.
     * @return True if no further handling is desired
     */
    virtual bool handleMessage(Message* msg) = 0;
  };

  /**
   * Creates a handler on `looper`.
   *
   * @param looper   the looper to run messages on.
   * @param callback a callback the caller owns and keeps alive; the handler only holds
   *                 it weakly, so it can detect that it has gone away. Pass null to
   *                 use `handleMessage()`.
   */
  static std::shared_ptr<EventHandler>
  create(const std::shared_ptr<EventLooper>& looper, const std::shared_ptr<Callback>& callback);

  explicit EventHandler(const std::shared_ptr<EventLooper>& looper, const std::shared_ptr<Callback>& callback) noexcept;

  virtual ~EventHandler();

  EventHandler(const EventHandler&) = delete;
  EventHandler& operator=(const EventHandler&) = delete;

  EventHandler(EventHandler&&) = delete;
  EventHandler& operator=(EventHandler&&) = delete;

  /**
   * Subclasses must implement this to receive messages.
   *
   * @param msg the message to handle. It is borrowed for the duration of the
   *            call; the queue owns it and releases it afterwards.
   */
  virtual void handleMessage(Message* msg) {
  }

  /**
   * Sets the callback that gets first refusal on every message.
   *
   * @param callback an object the caller also owns, or null to clear it.
   *
   * @attention This replaces a reference the looper thread may be reading as it
   * dispatches, and it is written under a lock so that the two cannot tear. It is
   * still meant to be called during setup, before the handler starts receiving
   * work; swapping a callback for one that is already being dispatched is a design
   * choice to make deliberately, not by accident.
   */
  void setCallback(const std::shared_ptr<Callback>& callback);

  /**
   * Whether a callback the handler can still reach is installed.
   *
   * Reports false once the callback has been destroyed, at which point messages
   * fall through to `handleMessage()` instead of being sent to a dead object.
   */
  [[nodiscard]] bool hasCallback() const noexcept;

  /**
   * Check if there are any pending posts of messages with code 'what' in the
   * message queue.
   */
  bool hasMessages(int what);

  /**
   * Pushes a message onto the end of the message queue after all pending
   * messages before the current time. It will be received in {@link
   * #handleMessage}, in the thread attached to this handler.
   *
   * @return Returns true if the message was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.
   */
  bool sendMessage(MessagePtr msg);

  /**
   * Enqueue a message into the message queue after all pending messages
   * before (current time + delayMillis). You will receive it in
   * {@link handleMessage}, in the thread attached to this handler.
   *
   * @return Returns true if the message was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.  Note that a
   *         result of true does not mean the message will be processed
   *         if the looper is quit before the delivery time of the message
   *         occurs then the message will be dropped.
   */
  bool sendMessageDelayed(MessagePtr msg, TimeDelta delay);

  /**
   * Enqueue a message into the message queue after all pending messages
   * before the absolute time (a `std::chrono::steady_clock` time point).
   * Time spent in deep sleep will add a delay to execution.
   * You will receive it in {@link handleMessage}, in the thread attached
   * to this handler.
   *
   * @param targetTime The absolute time at which the message should be
   *         delivered
   * @param async      when true, the message may be delivered while a
   *                   synchronisation barrier is held. See
   *                   {@link MessageQueue::setSyncBarrier}.
   *
   * @return Returns true if the message was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.
   *         Note that a result of true does not mean the message will be processed
   *         if the looper is quit before the delivery time of the message
   *         occurs then the message will be dropped.
   */
  bool sendMessageAtTime(MessagePtr msg, TimePoint targetTime, bool async = false);

  /**
   * Sends a Message containing only what value.
   *
   * @return Returns true if the message was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.
   */
  bool sendEmptyMessage(int what);

  /**
   * Sends a Message containing only what value, to be delivered
   * after the specified amount of time elapses.
   * @see #sendMessageDelayed(Message, TimeDelta)
   *
   * @return Returns true if the message was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.
   */
  bool sendEmptyMessageDelayed(int what, TimeDelta delay);

  /**
   * Sends a Message containing only what value, to be delivered
   * at a specific time.
   *
   * @return Returns true if the message was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.
   */
  bool sendEmptyMessageAtTime(int what, TimePoint targetTime);

  /**
   * Sends a Message containing only what value, asynchronously.
   *
   * An asynchronous message is delivered even while a synchronisation barrier is
   * held, which is how a VSYNC or an input event overtakes ordinary work.
   *
   * @return Returns true if the message was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.
   */
  bool sendEmptyMessageAsync(int what);

  /**
   * Sends a Message containing only what value, asynchronously, to be delivered
   * after the specified amount of time elapses.
   */
  bool sendEmptyMessageAsyncDelayed(int what, TimeDelta delay);

  /**
   * Sends a Message containing only what value, asynchronously, to be delivered
   * at a specific time.
   *
   * @see #sendMessageAtTime
   */
  bool sendEmptyMessageAtTimeAsync(int what, TimePoint targetTime);

  /**
   * Remove any pending posts of messages with code 'what' that are in the
   * message queue.
   */
  void removeMessages(int what);

  void removeAllCallbacksAndMessages();

  /**
   * Holds back every synchronous message on this handler's queue until the
   * returned guard is destroyed.
   *
   * Asynchronous messages keep being delivered while the barrier is held, which
   * is what a rendering loop needs: a VSYNC has to be able to restart the work
   * that ordinary messages would otherwise queue in front of.
   *
   * The barrier belongs to the calling thread, so only that thread can hold or
   * release it, and the guard releases it on every path out of the scope —
   * including an exception — so a barrier can never be leaked and stall the
   * queue forever.
   *
   * @return a guard that holds the barrier, or an inactive one when the handler
   *         is gone or another thread already holds a barrier on this queue
   *         (only one barrier can be held at a time).
   *
   * @see MessageQueue::setSyncBarrier
   */
  [[nodiscard]] BarrierGuard deriveSyncBarrier();

  /** Whether a synchronisation barrier is currently held on this handler's queue. */
  [[nodiscard]] bool isSyncBarrierSet() const;

  /**
   * How many messages a synchronisation barrier has held back on this handler's
   * queue so far.
   *
   * Monotonic, and it is the observable effect of a barrier: a caller can watch
   * this count move instead of sleeping and hoping a message had time to be held.
   * A synchronous message that the barrier stopped makes it go up; an
   * asynchronous one deliberately does not.
   */
  [[nodiscard]] uint64_t parkedByBarrierCount() const;

  /**
   * Causes the Closure to be added to the message queue. The closure will be
   * run on the thread to which this handler is attached.
   *
   * @param closure The Closure that will be executed.
   * @return Returns true if the Closure was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.
   */
  template<class Closure,
    typename std::enable_if<!std::is_convertible<
      Closure, std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  bool post(Closure&& closure) {
    return postTask(CreateQueuedTask(std::forward<Closure>(closure)));
  }

  /**
   * Causes the Closure to be added to the message queue, to be run
   * after the specified amount of time elapses.
   * The closure will be run on the thread to which this handler is attached.
   *
   * Time spent in deep sleep will add a delay to execution.
   *
   * @param closure The Closure that will be executed.
   * @param delay The delay until the Closure will be executed.
   *
   * @return Returns true if the Closure was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.  Note that a
   *         result of true does not mean the Closure will be processed --
   *         if the looper is quit before the delivery time of the message
   *         occurs then the message will be dropped.
   */
  template<class Closure,
    typename std::enable_if<!std::is_convertible<
      Closure, std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  bool postDelay(Closure&& closure, const TimeDelta delay) {
    return postTaskDelay(CreateQueuedTask(std::forward<Closure>(closure)), delay);
  }

  /**
   * Causes the Closure r to be added to the message queue, to be run
   * at a specific time.
   *
   * Time spent in deep sleep will add a delay to execution.
   * The Closure will be run on the thread to which this handler is attached.
   *
   * @param closure    The Closure that will be executed.
   * @param targetTime The absolute time at which the callback should run.
   *
   * @return Returns true if the Closure was successfully placed in to the
   *         message queue.  Returns false on failure, usually because the
   *         looper processing the message queue is exiting.  Note that a
   *         result of true does not mean the Closure will be processed -- if
   *         the looper is quit before the delivery time of the message
   *         occurs then the message will be dropped.
   */
  template<class Closure,
    typename std::enable_if<!std::is_convertible<
      Closure, std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  bool postAtTime(Closure&& closure, const TimePoint targetTime) {
    return postTaskAtTime(CreateQueuedTask(std::forward<Closure>(closure)), targetTime);
  }

  /**
   * A value that identifies this handler for message cancellation.
   *
   * A queued message records this instead of a pointer to its handler. A raw
   * pointer cannot be used: by the time a handler's destructor withdraws its
   * messages its `weak_ptr` has already expired, so a lookup through it returns
   * nothing and the comparison silently never matches. A value keeps working after
   * the handler is gone, which is exactly when the withdrawal has to happen.
   *
   * Messages are bound to the handler that posts them, and a handler withdraws
   * everything carrying its own id before it goes away, so an id cannot be
   * observed on a message belonging to someone else.
   */
  [[nodiscard]] uint64_t id() const noexcept {
    return id_;
  }

private:
  bool postTask(std::unique_ptr<QueuedTask> task);

  bool postTaskDelay(std::unique_ptr<QueuedTask> task, TimeDelta delay);

  bool postTaskAtTime(std::unique_ptr<QueuedTask> task, TimePoint targetTime);

  /**
   * The queue to post to, or null when nothing can be delivered any more.
   *
   * This is the single place the "is this handler still usable" question is
   * answered, so every send path treats a destroyed looper and a queue that has
   * been asked to quit the same way, and none of them race on a raw reference.
   */
  [[nodiscard]] std::shared_ptr<MessageQueue> queueIfUsable() const;

  /**
   * Handle system messages here.
   *
   * The message is borrowed for the duration of the call: the queue owns it and
   * releases it once the handler returns, and a pooled message goes back to the
   * pool at that point, so a receiver must not keep hold of the pointer.
   */
  void dispatchMessage(Message* msg);

  /**
   * Returns the shared pointer of the Handler
   */
  std::shared_ptr<EventHandler> getPtr() { return shared_from_this(); }

  static const char* getName() { return "Handler"; }

  friend class EventLooper;

  /**
   * The installed callback.
   *
   * Held weakly: the handler never owns the callback and never keeps it alive. A
   * strong reference would make the handler extend a callback the caller also owns,
   * and a weak reference is what lets a dispatch check that the callback still
   * exists before calling into it.
   *
   * Guarded by `callbackMutex`, because `setCallback()` can replace it while the
   * looper thread is dispatching.
   */
  std::weak_ptr<Callback> callback;

  /** Guards `callback`. Not taken on the hot path unless a callback is installed. */
  mutable std::mutex callbackMutex;

  /**
   * A barrier this handler is still holding.
   *
   * Counted rather than a flag, because barriers stack: deriving two of them from
   * the same handler has to leave the queue synchronised until *both* guards are
   * gone. A flag cannot tell the second release from the first, so the queue's
   * depth would never return to zero and every later barrier would be skipped —
   * leaving the queue synchronised for good.
   *
   * Shared between the guards, which may be destroyed on the same thread but in
   * any order, so the count is atomic.
   */
  std::atomic<int> barrierDepth{ 0 };

  /** The identity this handler stamps on its messages. See `id()`. */
  const uint64_t id_;

  std::weak_ptr<EventLooper> looper;
  std::weak_ptr<MessageQueue> msgQueue;
};

} // namespace coral

#endif // CORAL_EVENT_EVENT_HANDLER_H
