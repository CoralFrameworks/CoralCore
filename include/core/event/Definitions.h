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

#ifndef CORAL_EVENT_TIME_DELTA_H
#define CORAL_EVENT_TIME_DELTA_H

#include <chrono>

namespace coral {

/**
 * A span of time, in nanoseconds.
 *
 * An alias for the standard library type, so a duration is built and inspected with
 * `std::chrono` directly:
 *
 * @code
 *   TimeDelta delay = std::chrono::milliseconds(250);
 *   int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(delay).count();
 * @endcode
 *
 * There is deliberately no wrapper class: no conversion, no rounding and no member
 * functions to keep in step with the standard library.
 */
using TimeDelta = std::chrono::nanoseconds;


/**
 * A point on the monotonic (`std::chrono::steady_clock`) timeline.
 *
 * An alias for the standard library type, held as a nanosecond count since an
 * arbitrary epoch.
 *
 * WARNING:
 * This value must not be serialized across reboots or across devices: the reference
 * point is only stable for a given device between reboots.
 *
 * @code
 *   TimePoint now = std::chrono::steady_clock::now();
 *   TimePoint later = now + std::chrono::seconds(1);
 *   TimeDelta waited = later - now;
 * @endcode
 *
 * The `libs/core` module declares these two aliases for its own use as well, so that
 * neither module has to depend on the other. They name the same types, so values pass
 * between the two freely.
 */
using TimePoint = std::chrono::steady_clock::time_point;

} // namespace coral

#endif //CORAL_EVENT_TIME_DELTA_H
