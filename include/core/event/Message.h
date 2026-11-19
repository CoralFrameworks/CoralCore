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

#ifndef CORAL_EVENT_MESSAGE_H
#define CORAL_EVENT_MESSAGE_H

#include <core/event/MessagePool.h>
#include <core/event/QueuedTask.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace coral {

class EventHandler;

class MessageData {
public:
  MessageData() = default;
  virtual ~MessageData() = default;
};

template<class T>
class TypedMessageData : public MessageData {
public:
  explicit TypedMessageData(const T& data) noexcept: data_(data) {}

  ~TypedMessageData() override = default;

  const T& data() const { return data_; }

  T& data() { return data_; }

private:
  T data_;
};

template<class T>
inline std::unique_ptr<MessageData> WrapMessageData(const T& data) {
  return std::make_unique<TypedMessageData<T>>(data);
}

template<class T>
inline const T& UseMessageData(const std::unique_ptr<MessageData>& data) {
  return static_cast<TypedMessageData<T>*>(data.get())->data();
}

template<class T>
class ScopedMessageData : public MessageData {
public:
  explicit ScopedMessageData(std::unique_ptr<T> data) noexcept
      : data_(std::move(data)) {}

  ~ScopedMessageData() override = default;

  const std::unique_ptr<T>& data() const { return data_; }

  std::unique_ptr<T> data() { return data_; }

  const T& getData() const { return *data_; }

  T& getData() { return *data_; }

private:
  std::unique_ptr<T> data_;
};

template<class T>
class SharedMessageData : public MessageData {
public:
  explicit SharedMessageData(const std::shared_ptr<T>& data) noexcept
      : data_(data) {}

  const std::shared_ptr<T>& data() const { return data_; }

private:
  std::shared_ptr<T> data_;
};

template<class T>
inline std::unique_ptr<MessageData>
WrapSharedMsgData(const std::shared_ptr<T>& data) {
  return std::make_unique<SharedMessageData<T>>(data);
}

template<class T>
inline const std::shared_ptr<T>&
UseSharedMsgData(const std::unique_ptr<MessageData>& data) {
  return static_cast<SharedMessageData<T>*>(data.get())->data();
}

class Message final {
public:
  Message() = default;

  ~Message() = default;

  Message(const Message&) = delete;
  Message& operator=(const Message&) = delete;
  Message(Message&&) = delete;
  Message& operator=(Message&&) = delete;

  /**
   * User-defined message code so that the recipient can identify what this
   * message is about. Each Handler has its own name-space for message codes, so
   * you do not need to worry about yours conflicting with other handlers.
   */
  int what = 0;

  /**
   * arg1 and arg2 are lower-cost alternatives to using setData() if you only
   * need to store a few integer values.
   */
  int arg1 = 0;

  /**
   * rg1 and arg2 are lower-cost alternatives to using setData() if you only
   * need to store a few integer values.
   */
  int arg2 = 0;

  /**
   * An arbitrary object to send to the recipient
   */
  std::unique_ptr<MessageData> msgData;

  /**
   * The identity of the handler this message is addressed to.
   *
   * A value rather than a pointer because cancellation has to keep working while
   * the handler is being destroyed, when its weak reference can no longer be
   * locked. See `EventHandler::id()`.
   */
  uint64_t targetId = 0;

  /**
   * Withdraws this message.
   *
   * @return `true` when this call was the one that withdrew it, so a caller can
   *         tell "I cancelled it" from "someone already had".
   */
  bool cancel() noexcept {
    return !cancelled.exchange(true, std::memory_order_acq_rel);
  }

  /**
   * Obtains a message from the pool, with only its `what` set.
   *
   * This is the counterpart of Android's `Message.obtain()`: the returned object
   * comes from a free list whenever one is available, and returns to it when the
   * last owning pointer to it is destroyed. That covers every way a message can
   * end: delivered, cancelled, refused by a quitting queue, or never sent at all.
   *
   * @param handler the target, which may be null when the caller will bind one
   *                later.
   * @param what    the message code.
   */
  [[nodiscard]] static MessagePtr
  obtain(const std::shared_ptr<EventHandler>& handler, int what);

  [[nodiscard]] static MessagePtr
  obtain(const std::shared_ptr<EventHandler>& handler, int what, std::unique_ptr<MessageData> msgData);

  [[nodiscard]] static MessagePtr
  obtain(const std::shared_ptr<EventHandler>& handler, int what, int arg1, int arg2);

  [[nodiscard]] static MessagePtr
  obtain(const std::shared_ptr<EventHandler>& handler, int what, int arg1, int arg2,
      std::unique_ptr<MessageData> msgData);

  [[nodiscard]] static MessagePtr
  obtain(const std::shared_ptr<EventHandler>& handler,
      std::unique_ptr<QueuedTask> task);

  /**
   * Whether this message has been withdrawn by its owner.
   *
   * A queued message can be cancelled from any thread by a single atomic store,
   * which is what lets `MessageQueue::removeMessages()` work without a lock. The
   * looper checks this when it drains the message and drops it if it is set.
   *
   * @see MessageQueue::removeMessages
   */
  [[nodiscard]] bool isCancelled() const noexcept {
    return cancelled.load(std::memory_order_acquire);
  }

  /**
   * Whether this message is allowed to pass a synchronisation barrier.
   *
   * An asynchronous message — a VSYNC, an input event, anything that must not be
   * starved by ordinary work — is delivered even while every synchronous message
   * on the queue is being held back. See `MessageQueue::setSyncBarrier()`.
   */
  [[nodiscard]] bool isAsynchronous() const noexcept {
    return asynchronous;
  }

private:
  friend class EventHandler;
  friend class EventLooper;
  friend class MessageQueue;
  friend class MessagePool;

  /**
   * Takes a message out of the pool, or allocates one, and clears every field so
   * no state can leak from the previous use.
   *
   * @param handler the target to bind, or null to leave unbound.
   */
  [[nodiscard]] static MessagePtr create(const std::shared_ptr<EventHandler>& handler);

  /**
   * Marks this message as asynchronous.
   *
   * Private because whether a message may bypass a barrier is not something a
   * receiver gets to decide after the fact; it is a property of the post, so it
   * is set through the `EventHandler` send paths that take an `async` argument.
   */
  void setAsynchronous(const bool async) noexcept {
    asynchronous = async;
  }

  /** Returns every field to its initial state, for reuse out of the pool. */
  void reset() noexcept;

  /**
   * Everything {@link reset} clears that can run user code on destruction.
   *
   * The pool clears a message while holding its lock — it has to, because the free
   * list is threaded through the message — but a payload's destructor is arbitrary
   * user code that may itself post a message. The payloads are therefore *moved*
   * into one of these and destroyed after the lock is released.
   */
  struct Cleared {
    std::unique_ptr<MessageData> msgData;
    std::unique_ptr<QueuedTask> task;
  };

  /**
   * Returns every field to its initial state and hands back the payloads.
   *
   * The intrusive pool link is also cleared, which is what makes a message safe to
   * put back on the free list.
   */
  [[nodiscard]] Cleared clear() noexcept;

  /** The link that holds this message on the pool's free list. */
  Message* mPoolNext = nullptr;

  // binding to process target
  std::weak_ptr<EventHandler> target;

  // task implementation
  std::unique_ptr<QueuedTask> run_task_;

  /**
   * Set when the message is withdrawn.
   *
   * Atomic because the setter runs on whichever thread calls
   * `removeMessages()`, and the reader is the looper thread.
   */
  std::atomic<bool> cancelled{ false };

  /** Set when the message may bypass a synchronisation barrier. */
  bool asynchronous = false;
};

} // namespace coral

#endif // CORAL_EVENT_MESSAGE_H
