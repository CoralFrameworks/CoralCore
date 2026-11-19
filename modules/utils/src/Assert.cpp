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

#include <utils/Assert.h>
#include <utils/Panic.h>

#include <cstdlib>
#include <cstring>

namespace utils {

// we use a non-inlined, not marked as "no return" function for aborting so that we can set
// a breakpoint on the call to abort() in panic() below and skip over it in the debugger if
// needed.
void abort() noexcept {
  std::abort();
}

void panic(char const* func, char const* file, int line, char const* assertion, char const* msg) {
  if (strlen(msg) > 0) {
    PANIC_LOG("%s:%d: failed assertion '%s', message: '%s' \n", file, line, assertion, msg);
  } else {
    PANIC_LOG("%s:%d: failed assertion '%s'\n", file, line, assertion);
  }
  abort(); // set a breakpoint here
  return;  // this line is needed to be able to move the cursor here in the debugger
}

} // namespace utils
