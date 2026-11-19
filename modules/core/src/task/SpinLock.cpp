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

#include <core/task/SpinLock.h>
#include <utils/compiler.h>

// Before the namespace, not inside it. <emmintrin.h> pulls in <stdlib.h> on a Linux toolchain, and a
// C header included inside `namespace coral` declares every one of its names there: the next
// <cstdlib> inclusion then fails with "no member named 'abs' in the global namespace", because
// `using ::abs` cannot find an `abs` that was declared as `coral::abs`. MSVC's intrinsics header
// does not happen to expose a C library this way, which is why the placement only broke on Linux.
#if CORAL_CPU_SSE_LEVEL >= CORAL_CPU_SSE_LEVEL_SSE2
#include <emmintrin.h>
#endif

namespace coral {

// Renamed from "pause" to avoid conflict with function defined in unistd.h
#if CORAL_CPU_SSE_LEVEL >= CORAL_CPU_SSE_LEVEL_SSE2
static void do_pause() { _mm_pause(); }
#else

static void do_pause() { /*spin*/ }

#endif

void SpinLock::contendedAcquire() {
  while (locked.exchange(true, std::memory_order_acquire)) {
    do_pause();
  }
}

} // namespace coral
