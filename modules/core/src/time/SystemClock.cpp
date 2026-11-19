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

#include <core/time/SystemClock.h>

#include <chrono>
#include <ratio>

namespace coral {

int64_t SystemClock::CurrentTimeMillis() {
  const auto now = std::chrono::steady_clock::now();
  const std::chrono::duration<double, std::milli> ms = now.time_since_epoch();
  return static_cast<int64_t>(ms.count());
}

int64_t SystemClock::CurrentTimeMicros() {
  const auto now = std::chrono::steady_clock::now();
  const std::chrono::duration<double, std::micro> us = now.time_since_epoch();
  return static_cast<int64_t>(us.count());
}

int64_t SystemClock::CurrentTimeNanos() {
  const auto now = std::chrono::steady_clock::now();
  const std::chrono::duration<double, std::nano> ns = now.time_since_epoch();
  return static_cast<int64_t>(ns.count());
}

int64_t SystemClock::ElapsedRealtime() {
  const auto now = std::chrono::steady_clock::now();
  const std::chrono::duration<double, std::milli> ms = now.time_since_epoch();
  return static_cast<int64_t>(ms.count());
}

} // namespace coral
