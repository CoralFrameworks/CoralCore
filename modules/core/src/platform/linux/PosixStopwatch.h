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

#ifndef CORAL_PLATFORM_LINUX_POSIX_TIMER_H
#define CORAL_PLATFORM_LINUX_POSIX_TIMER_H

#include <core/time/Stopwatch.h>

#include <stdint.h>
#include <time.h>

namespace coral {

class PosixStopwatch : public Stopwatch {
public:
  PosixStopwatch() : Stopwatch(), mRunning(false), mStartTimeNs(0), mStopTimeNs(0) {
  }

  ~PosixStopwatch() override = default;

  void start() override;

  void stop() override;

  bool isStarted() override { return mRunning; }

  double getElapsedTime() override;

  double getAbsoluteTime() override;

private:
  bool mRunning;
  uint64_t mStartTimeNs;
  uint64_t mStopTimeNs;
};

} // namespace coral

#endif //CORAL_PLATFORM_LINUX_POSIX_TIMER_H
