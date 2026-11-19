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

#ifndef CORAL_TASK_THREAD_SAFETY_H
#define CORAL_TASK_THREAD_SAFETY_H

#if defined(__clang__)
#define CORAL_TASK_ANNOTATION_ATTRIBUTE_(x) __attribute__((x))
#else
#define CORAL_TASK_ANNOTATION_ATTRIBUTE_(x)  // no-op
#endif

#define CORAL_CAPABILITY(x) CORAL_TASK_ANNOTATION_ATTRIBUTE_(capability(x))

#define CORAL_SCOPED_CAPABILITY \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(scoped_lockable)

#define CORAL_GUARDED_BY(x) CORAL_TASK_ANNOTATION_ATTRIBUTE_(guarded_by(x))

#define CORAL_PT_GUARDED_BY(x) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(pt_guarded_by(x))

#define CORAL_ACQUIRED_BEFORE(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(acquired_before(__VA_ARGS__))

#define CORAL_ACQUIRED_AFTER(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(acquired_after(__VA_ARGS__))

#define CORAL_REQUIRES(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(requires_capability(__VA_ARGS__))

#define CORAL_REQUIRES_SHARED(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(requires_shared_capability(__VA_ARGS__))

#define CORAL_ACQUIRE(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(acquire_capability(__VA_ARGS__))

#define CORAL_ACQUIRE_SHARED(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(acquire_shared_capability(__VA_ARGS__))

#define CORAL_RELEASE(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(release_capability(__VA_ARGS__))

#define CORAL_RELEASE_SHARED(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(release_shared_capability(__VA_ARGS__))

#define CORAL_RELEASE_GENERIC(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(release_generic_capability(__VA_ARGS__))

#define CORAL_TRY_ACQUIRE(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(try_acquire_capability(__VA_ARGS__))

#define CORAL_TRY_ACQUIRE_SHARED(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(try_acquire_shared_capability(__VA_ARGS__))

#define CORAL_EXCLUDES(...) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(locks_excluded(__VA_ARGS__))

#define CORAL_ASSERT_CAPABILITY(x) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(assert_capability(x))

#define CORAL_ASSERT_SHARED_CAPABILITY(x) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(assert_shared_capability(x))

#define CORAL_RETURN_CAPABILITY(x) \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(lock_returned(x))

#define CORAL_NO_THREAD_SAFETY_ANALYSIS \
  CORAL_TASK_ANNOTATION_ATTRIBUTE_(no_thread_safety_analysis)

#endif //CORAL_TASK_THREAD_SAFETY_H
