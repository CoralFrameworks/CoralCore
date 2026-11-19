/*
 * Copyright (C) 2023 The Coral Project
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

#ifndef CORAL_TASK_MUTEX_H
#define CORAL_TASK_MUTEX_H

#include <core/task/TaskBase.h>
#include <core/task/ThreadSafety.h>

#include <mutex>
#include <shared_mutex>

namespace coral {

class ConditionVariable;

class CORAL_CAPABILITY("mutex") Mutex : public TaskBase {
public:
  Mutex() = default;
  ~Mutex() override = default;

  void lock() CORAL_ACQUIRE() { mutex_.lock(); }

  void unlock() CORAL_RELEASE() { mutex_.unlock(); }

private:
  std::mutex mutex_;

  friend class ConditionVariable;
};

class CORAL_CAPABILITY("mutex") RwMutex : public TaskBase {
public:
  RwMutex() = default;
  ~RwMutex() override = default;

  void lockWriter() CORAL_ACQUIRE() { mutex_.lock(); }

  void unlockWriter() CORAL_RELEASE() { mutex_.unlock(); }

  void lockReader() CORAL_ACQUIRE_SHARED() { mutex_.lock_shared(); }

  void unlockReader() CORAL_RELEASE_SHARED() { mutex_.unlock_shared(); }

private:
  std::shared_mutex mutex_;
};

class CORAL_SCOPED_CAPABILITY Lock : public TaskBase {
public:
  explicit Lock(Mutex& mutex) CORAL_ACQUIRE(mutex): mutex_(mutex) {
    mutex_.lock();
  }

  ~Lock() override CORAL_RELEASE() { mutex_.unlock(); }

private:
  Mutex& mutex_;
};

class CORAL_SCOPED_CAPABILITY ReaderLock : public TaskBase {
public:
  explicit ReaderLock(RwMutex& mutex) CORAL_ACQUIRE_SHARED(mutex)
    : mutex_(mutex) {
    mutex_.lockReader();
  }

  ~ReaderLock() override CORAL_RELEASE() { mutex_.unlockReader(); }

private:
  RwMutex& mutex_;
};

class CORAL_SCOPED_CAPABILITY WriterLock : public TaskBase {
public:
  explicit WriterLock(RwMutex& mutex) CORAL_ACQUIRE(mutex)
    : mutex_(mutex) {
    mutex_.lockWriter();
  }

  ~WriterLock() override CORAL_RELEASE() { mutex_.unlockWriter(); }

private:
  RwMutex& mutex_;
};

} // namespace coral

#endif //CORAL_TASK_MUTEX_H
