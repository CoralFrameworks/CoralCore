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

#ifndef CORAL_EVENT_MESSAGE_POOL_H
#define CORAL_EVENT_MESSAGE_POOL_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

namespace coral {

class Message;

/**
 * A free list of {@link Message} objects, the equivalent of Android's `Message`
 * pool.
 *
 * Posting a short message is otherwise two heap operations: one to allocate the
 * `Message` and one to destroy it once the message has been delivered or
 * cancelled. Under a burst of short posts that allocation traffic can cost more
 * than the work the messages carry. A message therefore returns to this pool
 * instead of to the allocator, and the next `Message::obtain()` takes it straight
 * back out.
 *
 * The free list is intrusive: the link lives inside the message, so a pooled
 * message costs no extra allocation and no extra node.
 *
 * @discussion
 * Recycling is *delayed deletion*. A message is only put back on the free list
 * by the deleter of the owning pointer, so it can only be reused after every
 * reference to it is gone. A message that is still queued, or still being
 * handled, is never on the list.
 *
 * Taking from and returning to the list is guarded by a short mutex rather than
 * being a lock-free stack. A Treiber stack over intrusive links has an ABA
 * window — two threads can pop and re-push the same node between another thread's
 * read and compare-exchange, which corrupts the list — and closing it would cost
 * either a tagged pointer or a reclamation scheme. The list operations are a few
 * pointer assignments, so the lock is far cheaper than the `malloc`/`free` pair
 * this replaces, and it is not on the message *delivery* path: the wait-free ring
 * in `MessageBuffer` still is.
 */
class MessagePool final {
public:
  /**
   * How many messages a pool keeps for reuse.
   *
   * The free list is not a cache of unbounded size: a burst that allocates ten
   * thousand messages must not pin ten thousand of them for the rest of the
   * process. Everything above this limit is destroyed, which is exactly what
   * would have happened without a pool.
   */
  static constexpr size_t kDefaultCapacity = 256;

  explicit MessagePool(const size_t capacity = kDefaultCapacity) noexcept
      : mCapacity(capacity) {}

  ~MessagePool();

  MessagePool(const MessagePool&) = delete;
  MessagePool& operator=(const MessagePool&) = delete;

  MessagePool(MessagePool&&) = delete;
  MessagePool& operator=(MessagePool&&) = delete;

  /**
   * The process-wide pool used by `Message::obtain()`.
   *
   * A function-local static, so it is built on first use. A message that outlives
   * it — one owned by another static object, say — still holds a reference to it,
   * so the pool itself stays alive until the last message is gone.
   */
  [[nodiscard]] static std::shared_ptr<MessagePool>& global() noexcept;

  /**
   * Takes a message off the free list, or allocates one when it is empty.
   *
   * @return a message with every field cleared, ready to be filled in.
   */
  [[nodiscard]] Message* obtain();

  /**
   * Hands `message` back for reuse.
   *
   * Called by the deleter of the owning pointer, so it runs on whichever thread
   * releases the last reference. Safe to call from any thread. A message that
   * cannot be pooled is destroyed rather than leaked.
   */
  void recycle(Message* message) noexcept;

  /** How many messages the pool currently holds. Approximate under concurrency. */
  [[nodiscard]] size_t available() const noexcept;

  /** How long the free list is allowed to grow. */
  [[nodiscard]] size_t capacity() const noexcept {
    return mCapacity;
  }

  /** How many `obtain()` calls were served from the free list. */
  [[nodiscard]] uint64_t reusedCount() const noexcept {
    return mReused.load(std::memory_order_relaxed);
  }

  /** How many messages had to be allocated. */
  [[nodiscard]] uint64_t allocatedCount() const noexcept {
    return mAllocated.load(std::memory_order_relaxed);
  }

  /** How many messages came back to the pool. */
  [[nodiscard]] uint64_t recycledCount() const noexcept {
    return mRecycled.load(std::memory_order_relaxed);
  }

  /**
   * Destroys everything the free list holds and zeroes the counters.
   *
   * The pool keeps working afterwards, so this is a reset rather than a shutdown:
   * it lets a test start from a known-empty pool, and the destructor uses it to
   * release what was cached. Messages that are still outstanding are unaffected —
   * they return to the emptied pool when their last owner lets go, exactly as
   * they would have otherwise.
   */
  void drain() noexcept;

private:
  /** Removes the head of the free list, or null when it is empty. Called with `mMutex` held. */
  [[nodiscard]] Message* popFreeLocked() noexcept;

  /** The link used while a message sits on the free list. */
  [[nodiscard]] static Message*& linkOf(Message* message) noexcept;

  /** Messages kept for reuse. */
  const size_t mCapacity;

  /**
   * Guards the free list.
   *
   * See the class comment: a lock-free list here would need ABA protection for a
   * saving measured in nanoseconds against the allocator it replaces. Mutable so
   * the const queries can read the list.
   */
  mutable std::mutex mMutex;

  /** Head of the free list. */
  Message* mFree = nullptr;

  /** Length of the free list. */
  size_t mSize = 0;

  std::atomic<uint64_t> mReused{ 0 };
  std::atomic<uint64_t> mAllocated{ 0 };
  std::atomic<uint64_t> mRecycled{ 0 };

  friend class Message;
};

/**
 * The deleter carried by every owner of a pooled message.
 *
 * The pointer does not own the message resource directly: it holds a reference to
 * the pool, and destroying the pointer returns the message to it. The pool is
 * held by `shared_ptr` so a message that outlives the pool falls back to an
 * ordinary delete instead of writing into freed memory.
 */
struct MessageRecycler {
  std::shared_ptr<MessagePool> pool;

  void operator()(Message* message) const noexcept;
};

/** The owning pointer type handed out by `Message::obtain()`. */
using MessagePtr = std::unique_ptr<Message, MessageRecycler>;

} // namespace coral

#endif // CORAL_EVENT_MESSAGE_POOL_H
