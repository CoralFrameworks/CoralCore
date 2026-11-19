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

#include <core/time/Stopwatch.h>

#if defined(ANDROID)
#include "platform/linux/PosixStopwatch.h"
#elif defined(APPLE)
#include "platform/darwin/DarwinStopwatch.h"
#elif defined(LINUX)
#include "platform/linux/PosixStopwatch.h"
#elif defined(WIN32)
#include "platform/win32/WindowsStopwatch.h"
#endif

namespace coral {

std::unique_ptr<Stopwatch> Stopwatch::Create() {
#if defined(ANDROID)
  return std::unique_ptr<Stopwatch>(new PosixStopwatch());
#elif defined(APPLE)
  return std::unique_ptr<Stopwatch>(new DarwinStopwatch());
#elif defined(LINUX)
  return std::unique_ptr<Stopwatch>(new PosixStopwatch());
#elif defined(WIN32)
  return std::unique_ptr<Stopwatch>(new WindowsStopwatch());
#else
  return nullptr;
#endif
}

} // namespace coral
