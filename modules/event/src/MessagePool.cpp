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

#include <core/event/MessagePool.h>

#include <core/event/Message.h>

namespace coral {

MessagePool::~MessagePool() {
  drain();
}

std::shared_ptr<MessagePool>& MessagePool::global() noexcept {
  // Built on first use and deliberately never destroyed. A function-local static
  // `shared_ptr` would be torn down at exit, and a message obtained afterwards —
  // from another static object's destructor, or from a looper thread that outlived
  // the rest of the program — would dereference a stale handle. Leaking one pool
  // is the cheap side of that trade, and the destructor still releases everything
  // the pool holds whenever it does run.
  static auto* const handle = new std::shared_ptr<MessagePool>(std::make_shared<MessagePool>());
  return *handle;
}

Message*& MessagePool::linkOf(Message* message) noexcept {
  // The pool is a friend of Message, so the intrusive link is reachable here
  // without exposing it on the public type.
  return message->mPoolNext;
}

size_t MessagePool::available() const noexcept {
  const std::lock_guard<std::mutex> guard(mMutex);
  return mSize;
}

Message* MessagePool::obtain() {
  // `cleared` lives outside the locked scope on purpose: its destructor runs the
  // payload destructors, which are arbitrary user code, and running them while the
  // pool is locked would deadlock the moment one of them posted a message.
  Message::Cleared cleared;
  {
    const std::lock_guard<std::mutex> guard(mMutex);
    if (auto* reused = popFreeLocked()) {
      mReused.fetch_add(1, std::memory_order_relaxed);
      // The clear is what breaks the link the free list is threaded through, so it
      // has to happen while the message is still ours and off the list.
      cleared = reused->clear();
      return reused;
    }
    mAllocated.fetch_add(1, std::memory_order_relaxed);
  }
  return new Message();
}

void MessagePool::recycle(Message* message) noexcept {
  if (message == nullptr) {
    return;
  }

  // Same reasoning as `obtain()`: the message is cleared, and its payloads moved
  // out of it, under the lock; the payloads are destroyed when this function
  // returns, after the lock is gone.
  Message::Cleared cleared;
  {
    const std::lock_guard<std::mutex> guard(mMutex);
    cleared = message->clear();

    // Once the list is at its limit the message is destroyed rather than stored,
    // which keeps the pool's footprint bounded. The payloads are already detached,
    // so this only releases the message itself; `cleared` still owns them.
    if (mSize >= mCapacity) {
      delete message;
      return;
    }

    linkOf(message) = mFree;
    mFree = message;
    ++mSize;
    mRecycled.fetch_add(1, std::memory_order_relaxed);
  }
}

Message* MessagePool::popFreeLocked() noexcept {
  auto* head = mFree;
  if (head == nullptr) {
    return nullptr;
  }
  mFree = linkOf(head);
  --mSize;
  // The link is cleared by the clear that follows, in `obtain()`.
  return head;
}

void MessagePool::drain() noexcept {
  // The list is detached under the lock; the messages are destroyed after it, for
  // the same reason the payloads are: their destructors can run user code.
  Message* message = nullptr;
  {
    const std::lock_guard<std::mutex> guard(mMutex);
    message = mFree;
    mFree = nullptr;
    mSize = 0;

    mReused.store(0, std::memory_order_relaxed);
    mAllocated.store(0, std::memory_order_relaxed);
    mRecycled.store(0, std::memory_order_relaxed);
  }

  while (message != nullptr) {
    auto* next = linkOf(message);
    delete message;
    message = next;
  }
}

void MessageRecycler::operator()(Message* message) const noexcept {
  if (message == nullptr) {
    return;
  }
  if (pool) {
    pool->recycle(message);
    return;
  }
  // No pool to return to, which happens for a message allocated during static
  // destruction or while the pool is draining: fall back to an ordinary delete.
  delete message;
}

} // namespace coral
