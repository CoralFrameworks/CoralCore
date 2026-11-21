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

// These must be compiler predefines, not CMake variables: CMake defines APPLE,
// LINUX and WIN32 only for its own scripts, never on the compiler command line.
// Testing them here made every macOS/iOS build fall through to `return nullptr`.
#if defined(__ANDROID__)
#include "platform/linux/PosixStopwatch.h"
#elif defined(__APPLE__)
#include "platform/darwin/DarwinStopwatch.h"
#elif defined(__linux__)
#include "platform/linux/PosixStopwatch.h"
#elif defined(_WIN32)
#include "platform/win32/WindowsStopwatch.h"
#endif

namespace coral {

std::unique_ptr<Stopwatch> Stopwatch::Create() {
#if defined(__ANDROID__)
  return std::unique_ptr<Stopwatch>(new PosixStopwatch());
#elif defined(__APPLE__)
  return std::unique_ptr<Stopwatch>(new DarwinStopwatch());
#elif defined(__linux__)
  return std::unique_ptr<Stopwatch>(new PosixStopwatch());
#elif defined(_WIN32)
  return std::unique_ptr<Stopwatch>(new WindowsStopwatch());
#else
  return nullptr;
#endif
}

} // namespace coral
