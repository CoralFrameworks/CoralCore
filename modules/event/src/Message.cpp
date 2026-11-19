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

#include <core/event/Message.h>

#include <core/event/EventHandler.h>

#include <utility>

namespace coral {

MessagePtr Message::create(const std::shared_ptr<EventHandler>& handler) {
  // Reuse out of the pool where possible; the pointer carries the recycler so the
  // message goes back the moment its last owner lets go, whichever way it ends.
  auto& pool = MessagePool::global();
  MessagePtr msg{ pool ? pool->obtain() : new Message(), MessageRecycler{ pool } };
  msg->target = handler;
  return msg;
}

void Message::reset() noexcept {
  // The payloads are released here rather than through `clear()`, which is the
  // form the pool uses when it needs them to outlive its lock.
  auto cleared = clear();
}

Message::Cleared Message::clear() noexcept {
  Cleared cleared{};
  what = 0;
  arg1 = 0;
  arg2 = 0;
  cleared.msgData = std::move(msgData);
  cleared.task = std::move(run_task_);
  msgData.reset();
  run_task_.reset();
  target.reset();
  targetId = 0;
  cancelled.store(false, std::memory_order_relaxed);
  asynchronous = false;
  mPoolNext = nullptr;
  return cleared;
}

MessagePtr
Message::obtain(const std::shared_ptr<EventHandler>& handler, const int what) {
  auto msg = create(handler);
  msg->what = what;
  return msg;
}

MessagePtr
Message::obtain(const std::shared_ptr<EventHandler>& handler, const int what, std::unique_ptr<MessageData> msgData) {
  auto msg = create(handler);
  msg->what = what;
  msg->msgData = std::move(msgData);
  return msg;
}

MessagePtr
Message::obtain(const std::shared_ptr<EventHandler>& handler, const int what, const int arg1, const int arg2) {
  auto msg = create(handler);
  msg->what = what;
  msg->arg1 = arg1;
  msg->arg2 = arg2;
  return msg;
}

MessagePtr
Message::obtain(const std::shared_ptr<EventHandler>& handler, const int what, const int arg1,
    const int arg2, std::unique_ptr<MessageData> msgData) {
  auto msg = create(handler);
  msg->what = what;
  msg->arg1 = arg1;
  msg->arg2 = arg2;
  msg->msgData = std::move(msgData);
  return msg;
}

MessagePtr
Message::obtain(const std::shared_ptr<EventHandler>& handler, std::unique_ptr<QueuedTask> task) {
  auto msg = create(handler);
  msg->run_task_ = std::move(task);
  return msg;
}

} // namespace coral
