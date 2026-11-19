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

#ifndef CORAL_TASK_CONDITION_VARIABLE_H
#define CORAL_TASK_CONDITION_VARIABLE_H

#include <core/task/Mutex.h>
#include <core/task/TaskBase.h>

#include <condition_variable>
#include <functional>

namespace coral {

/**
 * A condition variable exactly similar to the one in libcxx with two major differences:
 *
 * On the Wait, WaitFor, and WaitUntil calls, static analysis annotation are respected.
 * There is no ability to wait on a condition variable and also be susceptible to spurious wakes.
 * This is because the predicate is mandatory.
 */
class ConditionVariable final : public TaskBase {
public:
  ConditionVariable() = default;

  void notifyOne() { cv_.notify_one(); }

  void notifyAll() { cv_.notify_all(); }

  using Predicate = std::function<bool()>;

  void wait(Mutex& mutex) {
    std::unique_lock lock(mutex.mutex_, std::adopt_lock);
    cv_.wait(lock);
  }

  /**
   * Atomically unlocks the mutex and waits on the condition variable up to a specified time point.
   * Lock will be reacquired when the wait exits. Spurious wakes may happen before the time point
   * is reached. In such cases the predicate is invoked and it must return `false` for the wait
   * to continue. The predicate will be invoked with the mutex locked.
   *
   * Since the predicate is invoked with the mutex locked, if it accesses other guarded resources,
   * the predicate itself must be decorated with the CORAL_REQUIRES directive. For instance,
   * ```
   *   [] () CORAL_REQUIRES(mutex) {
   *     return my_guarded_resource.should_stop_waiting;
   *   }
   * ```
   *
   * @tparam Clock
   * @tparam Duration
   * @param mutex
   * @param time_point
   * @param should_stop_waiting
   * @return
   */
  template<class Clock, class Duration>
  bool waitUntil(Mutex& mutex,
    const std::chrono::time_point<Clock, Duration>& time_point,
    const Predicate& should_stop_waiting) CORAL_REQUIRES(mutex) {
    std::unique_lock lock(mutex.mutex_, std::adopt_lock);
    const auto result = cv_.wait_until(lock, time_point, should_stop_waiting);
    lock.release();
    return result;
  }

  /**
   * Atomically unlocks the mutex and waits on the condition variable for a designated duration.
   * Lock will be reacquired when the wait exits. Spurious wakes may happen before the time
   * point is reached. In such cases the predicate is invoked and it must return `false` for
   * the wait to continue. The predicate will be invoked with the mutex locked.
   *
   * Since the predicate is invoked with the mutex locked, if it accesses other guarded resources,
   * the predicate itself must be decorated with the CORAL_REQUIRES directive. For instance,
   * ```
   *   [] () CORAL_REQUIRES(mutex) {
   *     return my_guarded_resource.should_stop_waiting;
   *   }
   * ```
   *
   * @tparam Representation     The duration representation type.
   * @tparam Period             The duration period type.
   *
   * @param mutex               The mutex.
   * @param duration            The duration to wait for.
   * @param should_stop_waiting The predicate invoked on spurious wakes.
   *
   * @return The value of the predicate at the end of the wait.
   */
  template<class Representation, class Period>
  bool waitFor(Mutex& mutex,
    const std::chrono::duration<Representation, Period>& duration,
    const Predicate& should_stop_waiting) CORAL_REQUIRES(mutex) {
    return waitUntil(mutex, std::chrono::steady_clock::now() + duration, should_stop_waiting);
  }

  template<class Representation, class Period>
  bool waitFor(Mutex& mutex,
    const std::chrono::duration<Representation, Period>& duration) CORAL_REQUIRES(mutex) {
    return waitUntil(mutex, std::chrono::steady_clock::now() + duration, []() { return true; });
  }

  /**
   * Atomically unlocks the mutex and waits on the condition variable indefinitely till
   * the predicate determines that the wait must end. Lock will be reacquired when the wait
   * exits. Spurious wakes may happen before the time point is reached. In such cases the
   * predicate is invoked and it must return `false` for the wait to continue. The predicate
   * will be invoked with the mutex locked.
   *
   * Since the predicate is invoked with the mutex locked, if it accesses other guarded resources,
   * the predicate itself must be decorated with the CORAL_REQUIRES directive. For instance,
   * ```
   *   [] () CORAL_REQUIRES(mutex) {
   *     return my_guarded_resource.should_stop_waiting;
   *   }
   * ```
   *
   * @param mutex               The mutex
   * @param should_stop_waiting The should stop waiting
   */
  void wait(Mutex& mutex, const Predicate& should_stop_waiting) CORAL_REQUIRES(mutex) {
    std::unique_lock lock(mutex.mutex_, std::adopt_lock);
    cv_.wait(lock, should_stop_waiting);
    lock.release();
  }

private:
  std::condition_variable cv_;
};

} // namespace coral

#endif //CORAL_TASK_CONDITION_VARIABLE_H
