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

#ifndef CORAL_EVENT_MESSAGE_BUFFER_H
#define CORAL_EVENT_MESSAGE_BUFFER_H

#include "LockFreeMpscQueue.h"

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace coral {

/**
 * The growable hand-off between the threads posting messages and the looper
 * thread that drains them.
 *
 * The wait-free ring is the hot path and stays wait-free: as long as it has a
 * slot, posting takes no lock, allocates nothing and cannot fail. The ring is
 * bounded, though, and a bounded ring on a delivery path silently drops work the
 * moment it fills — the sender gets `false` and the message is gone.
 *
 * A full ring is therefore no longer a failure. The entry is appended to an
 * overflow list instead, which reallocates as it grows.
 *
 * @attention
 * Splitting one FIFO across two structures is where the subtlety is, and the
 * obvious rule — "the ring holds the older entries, so take from the ring
 * first" — is wrong. An entry reaches the overflow only because the ring was full
 * at that instant, and the ring can have been drained and refilled since: a post
 * that arrives *after* an overflow entry can take a ring slot freed in the
 * meantime. So the overflow can hold an older entry than the ring's head, in which
 * case taking the ring first delivers a later post before an earlier one.
 *
 * The two fronts are therefore compared by the `order` stamp their poster gave
 * them, and the older of the two is taken. That is not an optimisation that can be
 * relaxed: neither structure alone is ordered against the other, and the stamp is
 * the only thing that says which entry was posted first.
 *
 * The overflow is still the rare path, and the only one that takes a lock. The
 * mutex also does what the ring cannot do for itself: it makes the "is the
 * overflow empty?" answer stable, because only its holder can change it.
 *
 * @tparam Node the element type. It must be trivially copyable and at least as
 *              wide as a pointer, so it moves through a ring slot without tearing,
 *              and it must expose a `uint64_t order` member: the posting order
 *              used to compare the two fronts.
 */
template<typename Node>
class MessageBuffer final {
public:
  /**
   * @param ringCapacity slots in the wait-free ring. This many outstanding
   *                     entries never touch the overflow path.
   */
  explicit MessageBuffer(const size_t ringCapacity)
      : mRing(ringCapacity) {
  }

  MessageBuffer(const MessageBuffer&) = delete;
  MessageBuffer& operator=(const MessageBuffer&) = delete;

  /** The size of the wait-free ring, after rounding up to a power of two. */
  [[nodiscard]] size_t ringCapacity() const noexcept {
    return mRing.capacity();
  }

  /**
   * Publishes one entry.
   *
   * Never fails: a full ring spills into the overflow rather than reporting a
   * failure the caller would have to treat as a dropped message.
   */
  void push(const Node& node) {
    if (mRing.tryPush(node)) {
      return;
    }
    // The ring is full. This entry goes to the overflow with the stamp its poster
    // gave it, which is what lets the consumer put it back in order later.
    const std::lock_guard<std::mutex> guard(mSideMutex);
    mOverflow.push_back(node);
  }

  /**
   * Publishes one entry ahead of everything already queued.
   *
   * This exists for a consumer handing entries back: they were queued before
   * anything the consumer is about to see, so they have to go in front rather
   * than behind. The order stamps still decide delivery, so this only has to
   * place the entry on the older side.
   *
   * @attention Only the consumer may call this. Entries on the front are consumed
   * by `pop()`, so a producer using it would be inserting into the middle of the
   * queue from the wrong end.
   */
  void pushFront(const Node& node) {
    const std::lock_guard<std::mutex> guard(mSideMutex);
    mFront.push_back(node);
  }

  /**
   * Takes the oldest entry, if there is one.
   *
   * The ring's head and the two side fronts are compared by posting order and the
   * oldest is taken. See the class comment for why the ring cannot simply be
   * preferred, and why the side lists have to be examined on every call rather
   * than behind a flag: any flag is read before a producer that is about to
   * append, and a consumer that trusted it would go back to the ring and deliver
   * a later post first.
   *
   * @param out receives the entry. Untouched when there is none.
   * @return `false` when nothing is queued.
   */
  [[nodiscard]] bool pop(Node& out) {
    const Node* ringHead = nullptr;
    const bool hasRing = mRing.tryPeek(ringHead);

    {
      const std::lock_guard<std::mutex> guard(mSideMutex);
      if (!mOverflow.empty() && (!hasRing || mOverflow.front().order < ringHead->order)) {
        out = std::move(mOverflow.front());
        mOverflow.pop_front();
        return true;
      }
      if (!mFront.empty() && (!hasRing || mFront.front().order < ringHead->order)) {
        out = std::move(mFront.front());
        mFront.pop_front();
        return true;
      }
      if (!hasRing) {
        return false;
      }
    }

    // `tryPop` loses a race only against a producer that has claimed the head slot
    // and not published it yet; the peek above would not have reported it
    // otherwise. Reporting "nothing yet" is safe: the poster is mid-hand-off, and
    // the notification its post raises is what brings the consumer back.
    return mRing.tryPop(out);
  }

  /**
   * Visits the entries that are currently queued, without removing them.
   *
   * The order is oldest first, matching `pop()`. As with the ring underneath,
   * this tolerates a concurrent producer: a producer in the middle of a hand-off
   * can be missed, but nothing is reported twice and nothing is reported that is
   * not queued.
   *
   * @param visitor called as `visitor(const Node&)` for each queued entry. It
   *                must not free or reuse an entry.
   * @return the number of entries visited.
   */
  template<typename Visitor>
  std::size_t tryVisit(Visitor&& visitor) const {
    std::size_t visited = mRing.tryVisit(std::forward<Visitor>(visitor));
    {
      // The visitor may run for some time, and a producer or the consumer must not
      // be able to reallocate either list out from under the iteration. The front
      // list is written by the consumer, so a query from another thread has to take
      // this too: without it, `hasMessages()` would walk a deque the looper is
      // rewriting.
      const std::lock_guard<std::mutex> guard(mSideMutex);
      for (const auto& node: mOverflow) {
        visitor(static_cast<const Node&>(node));
        ++visited;
      }
      for (const auto& node: mFront) {
        visitor(static_cast<const Node&>(node));
        ++visited;
      }
    }
    return visited;
  }

private:
  /** The wait-free hot path. */
  LockFreeMpscQueue<Node> mRing;

  /**
   * Guards both side lists.
   *
   * One mutex rather than two: they are only ever touched together, on the cold
   * path, so a second lock would add an ordering rule for no gain.
   */
  mutable std::mutex mSideMutex;

  /** Entries pushed while the ring was full. */
  std::deque<Node> mOverflow;

  /**
   * Entries the consumer has taken back and put in front of the queue.
   *
   * Only the consumer calls `pushFront()`, and its entries always carry an older
   * stamp than anything a producer can add afterwards, which is why putting them
   * in front is enough to keep the order.
   */
  std::deque<Node> mFront;
};

} // namespace coral

#endif // CORAL_EVENT_MESSAGE_BUFFER_H
