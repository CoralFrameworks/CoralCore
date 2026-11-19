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

#ifndef CORAL_PLATFORM_DARWIN_TIMER_H
#define CORAL_PLATFORM_DARWIN_TIMER_H

#include <core/time/Stopwatch.h>

#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

namespace coral {

class DarwinStopwatch : public Stopwatch {
public:
  DarwinStopwatch() : Stopwatch(), mRunning(false), mStartTime(0), mStopTime(0), mSecondCoeff(0) {
  }

  ~DarwinStopwatch() override = default;

  bool isStarted() override { return mRunning; }

  void start() override;

  void stop() override;

  double getElapsedTime() override;

  double getAbsoluteTime() override;

private:
  double getSecondCoeff();

  bool mRunning;
  uint64_t mStartTime;
  uint64_t mStopTime;
  double mSecondCoeff;
};

} // namespace coral

#endif //CORAL_PLATFORM_DARWIN_TIMER_H
