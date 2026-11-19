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

#include "PosixStopwatch.h"

namespace coral {

namespace {

uint64_t GetCurrentTimeNs() {
  struct timespec currentTime;
  clock_gettime(CLOCK_MONOTONIC, &currentTime);
  return currentTime.tv_sec * 1'000'000'000llu + currentTime.tv_nsec;
}

}

void PosixStopwatch::start() {
  mStartTimeNs = GetCurrentTimeNs();
  mRunning = true;
}

void PosixStopwatch::stop() {
  mStopTimeNs = GetCurrentTimeNs();
  mRunning = false;
}

double PosixStopwatch::getElapsedTime() {
  uint64_t endTimeNs;
  if (mRunning) {
    endTimeNs = GetCurrentTimeNs();
  } else {
    endTimeNs = mStopTimeNs;
  }
  return (endTimeNs - mStartTimeNs) * 1e-9;
}

double PosixStopwatch::getAbsoluteTime() {
  return GetCurrentTimeNs() * 1e-9;
}

} // namespace coral
