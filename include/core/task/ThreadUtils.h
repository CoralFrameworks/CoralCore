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

#ifndef CORAL_TASK_UTILS_H
#define CORAL_TASK_UTILS_H

#include <chrono>
#include <cstdint>
#include <thread>

namespace coral {

class ThreadUtils {
public:
  enum class Priority : uint8_t {
    kAudio,
    kUrgentAudio,
    kNormal,
    kDisplay,
    kUrgentDisplay,
    kBackground,
  };

  static std::thread::id getThreadId() noexcept;

  static bool isThisThread(std::thread::id id) noexcept;

  static void setThreadName(const char* name) noexcept;

  static void setThreadPriority(Priority priority) noexcept;

  static void elevateCurrentThread() noexcept;

  static void sleepUntilSteady(std::chrono::steady_clock::time_point deadline) noexcept;
};

} // namespace coral

#endif //CORAL_TASK_UTILS_H
