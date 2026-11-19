/*
 * Copyright (C) 2022 The Coral Project
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

#ifndef CORAL_EVENT_QUEUED_TASK_H
#define CORAL_EVENT_QUEUED_TASK_H

#include <memory>
#include <type_traits>
#include <utility>

namespace coral {

class QueuedTask {
public:
  virtual ~QueuedTask() = default;

  /**
   * Main routine that will run when the task is executed on the desired queue.
   *
   * @return  The task should return |true| to indicate that it should be
   * deleted or |false| to indicate that the queue should consider ownership of
   * the task having been transferred.  Returning |false| can be useful if a
   * task has re-posted itself to a different queue or is otherwise being
   * re-used.
   */
  virtual bool run() = 0;
};

namespace details {

/**
 * Simple implementation of QueuedTask for use with lambdas.
 */
template<typename Closure>
class ClosureTask : public QueuedTask {
public:
  explicit ClosureTask(Closure&& closure) noexcept
      : closure_(std::forward<Closure>(closure)) {}

private:
  bool run() override {
    closure_();
    return true;
  }

  std::decay_t<Closure> closure_;
};

} // namespace details

/**
 * Convenience function to construct closures that can be passed directly
 * to methods that support std::unique_ptr<QueuedTask> but not template
 * based parameters.
 */
template<typename Closure>
std::unique_ptr<QueuedTask> CreateQueuedTask(Closure&& closure) {
  return std::make_unique<details::ClosureTask<Closure>>(std::forward<Closure>(closure));
}

} // namespace coral

#endif // CORAL_EVENT_QUEUED_TASK_H
