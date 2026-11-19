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

#ifndef CORAL_TASK_BASE_H
#define CORAL_TASK_BASE_H

#include <functional>

namespace coral {

using closure = std::function<void()>;

class TaskBase {
public:
  TaskBase(const TaskBase&) = delete;
  TaskBase& operator=(const TaskBase&) = delete;

  TaskBase(TaskBase&&) = delete;
  TaskBase& operator=(TaskBase&&) = delete;

protected:
  TaskBase() = default;
  virtual ~TaskBase() = default;
};

} // namespace coral

#endif //CORAL_TASK_BASE_H
