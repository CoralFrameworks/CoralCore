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

#ifndef CORAL_EVENT_LOCK_FREE_MPSC_QUEUE_H
#define CORAL_EVENT_LOCK_FREE_MPSC_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace coral {

/**
 * A bounded wait-free-producer / lock-free-consumer queue, used by the message
 * queue to move posts from any thread to the looper thread without a lock.
 *
 * The algorithm is the standard bounded MPMC ring: each slot carries a
 * monotonically increasing sequence number, a producer claims the next slot with
 * a single compare-exchange, and a consumer releases it by publishing the
 * sequence for the next lap. There is no critical section, no lock, and no
 * pointer reclamation problem, because the storage is fixed and every slot is
 * handed over exactly once.
 *
 * The queue is *bounded*, which is what makes it allocation-free and
 * reclamation-free. `tryPush()` returns false when it is full instead of growing.
 *
 * @tparam Node the element type. It must be at least as wide as a pointer so it
 *              can be moved through a slot without tearing; the message queue
 *              uses a pointer-sized handle regardless of the message size.
 */
template<typename Node>
class LockFreeMpscQueue final {
public:
  /**
   * @param capacity the number of slots. Rounded up to a power of two, and at
   *                 least 2. This is the maximum number of undelivered elements.
   */
  explicit LockFreeMpscQueue(const size_t capacity)
      : slots_(std::make_unique<Slot[]>(roundUpToPowerOfTwo(capacity))) {
    const size_t count = roundUpToPowerOfTwo(capacity);
    mask_ = count - 1;
    // Lap 0 is claimable by the producer, so every slot starts empty.
    for (size_t i = 0; i < count; ++i) {
      slots_[i].sequence_.store(i, std::memory_order_relaxed);
    }
  }

  LockFreeMpscQueue(const LockFreeMpscQueue&) = delete;
  LockFreeMpscQueue& operator=(const LockFreeMpscQueue&) = delete;

  /** The usable slot count, after rounding. */
  [[nodiscard]] size_t capacity() const noexcept {
    return mask_ + 1;
  }

  /**
   * Publishes one element, if there is room.
   *
   * @return `false` when the queue is full, so the caller can report the failure
   *         rather than block or allocate.
   */
  [[nodiscard]] bool tryPush(const Node& node) noexcept {
    std::size_t position = head_.load(std::memory_order_relaxed);
    for (;;) {
      Slot& slot = slots_[position & mask_];
      const std::size_t sequence = slot.sequence_.load(std::memory_order_acquire);
      const auto difference = static_cast<std::ptrdiff_t>(sequence) - static_cast<std::ptrdiff_t>(position);
      if (difference == 0) {
        // The slot is empty and this is our lap: claim it.
        if (head_.compare_exchange_weak(position, position + 1, std::memory_order_relaxed,
                std::memory_order_relaxed)) {
          slot.node_ = node;
          // Releasing the sequence publishes the payload to the consumer.
          slot.sequence_.store(position + 1, std::memory_order_release);
          return true;
        }
        // Another producer claimed it; `position` was refreshed by the CAS.
        continue;
      }
      if (difference < 0) {
        // The consumer has not freed this lap's slot yet, so the ring is full.
        return false;
      }
      // The slot already moved on: reload the head and try again.
      position = head_.load(std::memory_order_relaxed);
    }
  }

  /**
   * Takes the oldest element, if any.
   *
   * @param out receives the element. Untouched when there is none.
   * @return `false` when the queue is empty.
   */
  [[nodiscard]] bool tryPop(Node& out) noexcept {
    const std::size_t position = tail_.load(std::memory_order_relaxed);
    Slot& slot = slots_[position & mask_];
    const std::size_t sequence = slot.sequence_.load(std::memory_order_acquire);
    if (static_cast<std::ptrdiff_t>(sequence) - static_cast<std::ptrdiff_t>(position) != 1) {
      return false;
    }
    out = slot.node_;
    // Freeing the slot for the next lap is what publishes the pop.
    slot.sequence_.store(position + mask_ + 1, std::memory_order_release);
    tail_.store(position + 1, std::memory_order_relaxed);
    return true;
  }

  /**
   * Reads the oldest element without removing it.
   *
   * @param out receives the element. Untouched when there is none.
   * @return `false` when the queue is empty, or when a producer has claimed the
   *         slot it would have to read and has not published it yet. Either way
   *         there is nothing that can be reported as the head yet.
   *
   * @discussion
   * This exists so a consumer that keeps a second structure alongside the ring can
   * decide which of the two holds the older element before taking either. It reads
   * only the slot the consumer owns, so it is safe against a concurrent producer:
   * a producer can fill a *newer* slot while this runs, which cannot change the
   * element this call reports.
   */
  [[nodiscard]] bool tryPeek(const Node*& out) const noexcept {
    const std::size_t position = tail_.load(std::memory_order_relaxed);
    const Slot& slot = slots_[position & mask_];
    const std::size_t sequence = slot.sequence_.load(std::memory_order_acquire);
    if (static_cast<std::ptrdiff_t>(sequence) - static_cast<std::ptrdiff_t>(position) != 1) {
      return false;
    }
    out = &slot.node_;
    return true;
  }

  /** Whether the queue currently holds nothing. */
  [[nodiscard]] bool empty() const noexcept {
    const std::size_t position = tail_.load(std::memory_order_relaxed);
    const Slot& slot = slots_[position & mask_];
    const std::size_t sequence = slot.sequence_.load(std::memory_order_acquire);
    return static_cast<std::ptrdiff_t>(sequence) - static_cast<std::ptrdiff_t>(position) != 1;
  }

  /**
   * Visits the elements that are currently queued, without removing them.
   *
   * The visit runs from the oldest element towards the newest and stops at the
   * first cell that is not committed, so it may miss the newest elements if a
   * producer is mid-hand-off. It never reports an element twice and never
   * reports one that is not in the queue.
   *
   * This exists so that a query such as "is a message for this handler queued?"
   * can be answered without draining and reposting the ring, which would reorder
   * concurrent posts. It is safe against a concurrent producer: the cell's
   * sequence is loaded with acquire, so a payload read after it is the payload
   * that producer published for that lap.
   *
   * @param visitor called as `visitor(const Node&)` for each queued element.
   * @return the number of elements visited.
   *
   * @discussion
   * The caller must not free or reuse an element from inside the visitor, and
   * must tolerate the visitor running concurrently with a producer that is
   * adding a newer element.
   */
  template<typename Visitor>
  std::size_t tryVisit(Visitor&& visitor) const noexcept {
    std::size_t visited = 0;
    for (std::size_t position = tail_.load(std::memory_order_acquire);; ++position) {
      const Slot& slot = slots_[position & mask_];
      const std::size_t before = slot.sequence_.load(std::memory_order_acquire);
      if (static_cast<std::ptrdiff_t>(before) - static_cast<std::ptrdiff_t>(position) != 1) {
        // Either the cell is free, or a producer claimed it and has not finished
        // publishing: stopping here is what keeps the read from tearing.
        break;
      }
      // The slot was published for this lap, so reading the payload now is safe
      // from the producer. The consumer is the other risk: it can pop this very
      // slot and let a producer refill the same cell for the next lap while the
      // payload below is being read, because the ring was full and reused the
      // index. Re-reading the sequence afterwards detects that — the cell's
      // sequence for this lap is gone either way, whether it was freed or claimed
      // again — and skipping the cell is correct, because an entry that was
      // consumed while this ran is no longer something this call has to report.
      const Node& node = slot.node_;
      const std::size_t after = slot.sequence_.load(std::memory_order_acquire);
      if (after != before) {
        continue;
      }
      visitor(node);
      ++visited;
      if (visited >= mask_ + 1) {
        // A full lap: everything currently queued has been visited.
        break;
      }
    }
    return visited;
  }

private:
  /** Rounds up to the smallest power of two that is at least `capacity`, and 2. */
  [[nodiscard]] static size_t roundUpToPowerOfTwo(size_t capacity) noexcept {
    size_t rounded = 2;
    while (rounded < capacity) {
      rounded <<= 1;
    }
    return rounded;
  }

  struct alignas(64) Slot {
    std::atomic<std::size_t> sequence_{ 0 };
    Node node_{};
  };

  /**
   * A fixed array rather than a vector: a slot holds an atomic, so it must not be
   * copied or moved, and a fixed size states that plainly.
   */
  std::unique_ptr<Slot[]> slots_;
  std::size_t mask_ = 0;
  alignas(64) std::atomic<std::size_t> head_{ 0 };
  alignas(64) std::atomic<std::size_t> tail_{ 0 };
};

} // namespace coral

#endif // CORAL_EVENT_LOCK_FREE_MPSC_QUEUE_H
