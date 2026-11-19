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

#ifndef CORAL_TIME_STOPWATCH_H
#define CORAL_TIME_STOPWATCH_H

#include <memory>

namespace coral {

/**
 * A monotonic stopwatch.
 *
 * `start`/`stop` bracket an interval and `getElapsedTime` reports it in seconds; `getAbsoluteTime`
 * exposes a monotonic timestamp whose origin is unspecified. It does not schedule callbacks and has
 * no thread affinity: for a callback/repeating timer on an event loop use `event::Timer`.
 */
class Stopwatch {
public:
  virtual ~Stopwatch() = default;

  static std::unique_ptr<Stopwatch> Create();

  virtual void start() = 0;

  virtual void stop() = 0;

  virtual bool isStarted() { return false; }

  virtual double getElapsedTime() { return 0; }

  /**
   * Timestamp functionality: Use getAbsoluteTime() to get an absolute time with an unknown
   * origin. This time moves forward regardless of start()/stop().
   */
  virtual double getAbsoluteTime() { return 0; }
};

} // namespace coral

#endif //CORAL_TIME_STOPWATCH_H
