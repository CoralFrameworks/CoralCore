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

#ifndef CORAL_TASK_EINTR_WRAPPER_H
#define CORAL_TASK_EINTR_WRAPPER_H

#include <cerrno>

#if defined(_WIN32)

// Windows has no concept of EINTR.
#define CORAL_HANDLE_EINTR(x) (x)
#define CORAL_IGNORE_EINTR(x) (x)

#else

#define CORAL_HANDLE_EINTR(x)                               \
  ({                                                        \
    decltype(x) eintr_wrapper_result;                       \
    do {                                                    \
      eintr_wrapper_result = (x);                           \
    } while (eintr_wrapper_result == -1 && errno == EINTR); \
    eintr_wrapper_result;                                   \
  })

#define CORAL_IGNORE_EINTR(x)                               \
  ({                                                        \
    decltype(x) eintr_wrapper_result;                       \
    do {                                                    \
      eintr_wrapper_result = (x);                           \
      if (eintr_wrapper_result == -1 && errno == EINTR) {   \
        eintr_wrapper_result = 0;                           \
      }                                                     \
    } while (0);                                            \
    eintr_wrapper_result;                                   \
  })

#endif  // defined(_WIN32)

#endif //CORAL_TASK_EINTR_WRAPPER_H
