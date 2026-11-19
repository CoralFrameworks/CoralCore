/*
 * Copyright (C) 2026 The Coral Project
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

#ifndef CORAL_OBSERVER_EXECUTOR_H
#define CORAL_OBSERVER_EXECUTOR_H

#include <functional>
#include <utility>

namespace coral::observer {

/**
 * Where an observation callback is delivered.
 *
 * An observation without an executor calls its callback on the thread that changed the property or
 * emitted the event. An executor moves that callback to a serial queue, such as a GUI main loop, a
 * player transport thread, or an event thread. The executor owns the ordering and the lifetime of
 * the task it was given; the observer module does not create a thread of its own.
 */
class Executor {
public:
  virtual ~Executor() = default;

  /** Queues @p task for execution. The task must not be dropped. */
  virtual void post(std::function<void()> task) = 0;
};

/** An executor that runs the task immediately on the calling thread. */
class InlineExecutor final : public Executor {
public:
  void post(std::function<void()> task) override {
    if (task != nullptr) {
      task();
    }
  }
};

} // namespace coral::observer

#endif // CORAL_OBSERVER_EXECUTOR_H
