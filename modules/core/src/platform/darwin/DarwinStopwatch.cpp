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

#include "DarwinStopwatch.h"

namespace coral {

void DarwinStopwatch::start() {
  mStartTime = mach_absolute_time();
  // Cache secondCoeff
  getSecondCoeff();
  mRunning = true;
}

void DarwinStopwatch::stop() {
  mStopTime = mach_absolute_time();
  mRunning = false;
}

double DarwinStopwatch::getElapsedTime() {
  if (mRunning) {
    return mSecondCoeff * (mach_absolute_time() - mStartTime);
  } else {
    return mSecondCoeff * (mStopTime - mStartTime);
  }
}

double DarwinStopwatch::getAbsoluteTime() {
  return getSecondCoeff() * mach_absolute_time();
}

double DarwinStopwatch::getSecondCoeff() {
  // If this is the first time we've run, get the timebase.
  if (mSecondCoeff == 0.0) {
    mach_timebase_info_data_t timebaseInfo;
    mach_timebase_info(&timebaseInfo);
    mSecondCoeff = timebaseInfo.numer * (1.0 / 1000000000) / timebaseInfo.denom;
  }
  return mSecondCoeff;
}

} // namespace coral
