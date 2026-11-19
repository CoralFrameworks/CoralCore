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

#ifndef CORAL_EVENT_EVENT_LOOPER_H
#define CORAL_EVENT_EVENT_LOOPER_H

#include <memory>
#include <thread>

namespace coral {

class MessageQueue;

/**
 * Class used to run a message loop for a thread.  Threads by default do
 * not have a message loop associated with them; to create one, call
 * {@link EventLooper::prepare()} in the thread that is to run the loop, and then
 * {@link EventLooper::loop()} to have it process messages until the loop is stopped.
 */
class EventLooper final {
public:
  /**
   * Initialize the current thread as a looper.
   * This gives you a chance to create handlers that then reference
   * this looper, before actually starting the loop. Be sure to call
   * {@link loop()} after calling this method, and end it by calling
   * {@link quit()}.
   */
  static void prepare();

  /**
   * Run the message queue in this thread. Be sure to call
   * {@link quit()} to end the loop.
   */
  static void loop();

  /**
   * Return the Looper object associated with the current thread.
   *
   * Returns nullptr if the calling thread is not associated with a Looper.
   */
  static const std::shared_ptr<EventLooper>& myLooper();

  /**
   * Returns the {@link MessageQueue} object associated with the current
   * thread.
   *
   * This must be called from a thread running a Looper, otherwise returns
   * nullptr
   */
  static const std::shared_ptr<MessageQueue>& myQueue();

  /**
   * Whether a Looper is associated with the calling thread.
   *
   * Cheaper and clearer than comparing `myLooper()` against nullptr, and it does
   * not touch the shared pointer.
   */
  [[nodiscard]] static bool hasLooper() noexcept;

  /**
   * Whether the calling thread's own queue is currently holding a
   * synchronisation barrier back.
   *
   * Always false for a thread without a looper. A barrier holds back synchronous
   * messages while letting asynchronous ones through; use
   * {@link EventHandler::deriveSyncBarrier} to set one, since a barrier belongs to
   * the thread that takes it and needs somewhere to be released from.
   */
  [[nodiscard]] static bool isSyncBarrierSet() noexcept;

  virtual ~EventLooper();

  /**
   * Quits the looper.
   *
   * Causes the {@link loop()} method to terminate without processing any
   * more messages in the message queue.
   *
   * Any attempt to post messages to the queue after the looper is asked to
   * quit will fail. For example, the {@link Handler#sendMessage(Message)}
   * method will return false.
   *
   * Using this method may be unsafe because some messages may not be
   * delivered before the looper terminates.  Consider using
   * {@link quitSafety} instead to ensure that all pending work is completed
   * in an orderly manner.
   *
   * @see #quitSafety
   */
  void quit();

  /**
   * Quits the looper safely.
   *
   * Causes the {@link loop()} method to terminate as soon as all remaining
   * messages in the message queue that are already due to be delivered have
   * been handled. However, pending delayed messages with due times in the
   * future will not be delivered before the loop terminates.
   *
   * Any attempt to post messages to the queue after the looper is asked to
   * quit will fail. For example, the {@link Handler#sendMessage(Message)}
   * method will return false.
   */
  void quitSafety();

  /**
   * Returns true if the current thread is this looper's thread.
   */
  [[nodiscard]] bool isCurrentThread() const;

private:
  explicit EventLooper(std::thread::id threadId, bool quitAllowed) noexcept;

  class Holder;

  static Holder& get() noexcept;

  static void remove(const std::thread::id& id);

  static const char* getName() { return "Looper"; }

  friend class EventHandler;

  friend class EventThread;

  std::thread::id threadId;
  std::shared_ptr<MessageQueue> msgQueue;
};

} // namespace coral

#endif // CORAL_EVENT_EVENT_LOOPER_H
