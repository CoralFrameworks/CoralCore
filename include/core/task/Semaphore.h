/*
 * Copyright (C) 2025 The Coral Project
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


#ifndef CORAL_TASK_SEMAPHORE_H
#define CORAL_TASK_SEMAPHORE_H

#include <core/task/TaskBase.h>

#include <memory>

namespace coral {

class PlatformSemaphore;

/**
 * A traditional counting semaphore.  `wait`s decrement the counter and `signal` increments it.
 *
 * This is a cross-platform replacement for std::counting_semaphore
 * which is only available since C++20. Once Coral migrates past
 * that point, this class should become obsolete and must be replaced.
 */
class Semaphore final : public TaskBase {
public:
  /**
   * Initializes the counting semaphore to a specified start count.
   *
   * Callers must check if the handle could be successfully created
   * by calling the `isValid` method. `wait`s on an invalid
   * semaphore will always fail and signals will fail silently.
   *
   * @param count The starting count of the counting semaphore.
   */
  explicit Semaphore(uint32_t count = 0) noexcept;

  /**
   * Destroy the counting semaphore.
   */
  ~Semaphore() override;

  /**
   * Check if the underlying semaphore handle could be created.
   * Failure modes are platform specific and may occur due to issue
   * like handle exhaustion. All `wait`s on invalid semaphore
   * handles will fail and `signal` calls will be ignored.
   *
   * @return    True if valid, False otherwise.
   */
  [[nodiscard]] bool isValid() const;

  /**
   * Decrements the count and waits indefinitely if the value is
   * less than zero for a `signal`.
   *
   * @return    If the `wait` call was successful. See `isValid` for failure.
   */
  [[nodiscard]] bool wait() const;

  /**
   * Decrement the counts if it is greater than zero. Returns false
   * if the counter is already at zero.
   *
   * @return    If the count could be decremented.
   */
  [[nodiscard]] bool tryWait() const;

  /**
   * Increment the count by one.
   * Any pending `wait`s will be resolved at this point.
   */
  void signal() const;

private:
  std::unique_ptr<PlatformSemaphore> impl_;
};

} // namespace coral

#endif //CORAL_TASK_SEMAPHORE_H
