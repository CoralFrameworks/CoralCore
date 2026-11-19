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

#include <core/task/ThreadUtils.h>

#include <minilog/log.h>
#include <utils/compiler.h>

#include <cstring>
#include <string>

#if defined(__APPLE__)
#include <pthread.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#elif defined(WIN32)
#define NOMINMAX
#include <windows.h>
#include <avrt.h>   // MMCSS
#pragma comment(lib, "avrt.lib")
#endif

#ifdef __ANDROID__
// see https://developer.android.com/topic/performance/threads#priority
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#ifndef ANDROID_PRIORITY_URGENT_AUDIO
#define ANDROID_PRIORITY_URGENT_AUDIO (-19)
#endif
#ifndef ANDROID_PRIORITY_AUDIO
#define ANDROID_PRIORITY_AUDIO (-16)
#endif
#ifndef ANDROID_PRIORITY_URGENT_DISPLAY
#define ANDROID_PRIORITY_URGENT_DISPLAY (-8)
#endif
#ifndef ANDROID_PRIORITY_DISPLAY
#define ANDROID_PRIORITY_DISPLAY (-4)
#endif
#ifndef ANDROID_PRIORITY_NORMAL
#define ANDROID_PRIORITY_NORMAL (0)
#endif
#ifndef ANDROID_PRIORITY_BACKGROUND
#define ANDROID_PRIORITY_BACKGROUND (10)
#endif
#elif defined(__linux__)
// There is no glibc wrapper for gettid on linux so we need to syscall it.
#include <unistd.h>
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

namespace coral {

void ThreadUtils::setThreadName(char const* name) noexcept {
#if defined(__linux__)
  pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
  pthread_setname_np(name);
#elif defined(WIN32)
  std::string_view u8name(name);
  size_t size = MultiByteToWideChar(CP_UTF8, 0, u8name.data(), u8name.size(), nullptr, 0);

  std::wstring u16name;
  u16name.resize(size);
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8name.data(), u8name.size(), u16name.data(), u16name.size());

  SetThreadDescription(GetCurrentThread(), u16name.data());
#endif
}

void ThreadUtils::setThreadPriority(const Priority priority) noexcept {
#ifdef __ANDROID__
  int androidPriority = 0;
  switch (priority) {
    case Priority::kAudio:
      androidPriority = ANDROID_PRIORITY_AUDIO;
      break;
    case Priority::kUrgentAudio:
      androidPriority = ANDROID_PRIORITY_URGENT_AUDIO;
      break;
    case Priority::kBackground:
      androidPriority = ANDROID_PRIORITY_BACKGROUND;
      break;
    case Priority::kNormal:
      androidPriority = ANDROID_PRIORITY_NORMAL;
      break;
    case Priority::kDisplay:
      androidPriority = ANDROID_PRIORITY_DISPLAY;
      break;
    case Priority::kUrgentDisplay:
      androidPriority = ANDROID_PRIORITY_URGENT_DISPLAY;
      break;
  }
  errno = 0;
  int error = setpriority(PRIO_PROCESS, 0, androidPriority);
#ifndef NDEBUG
  if (CORAL_UNLIKELY(error)) {
    MINILOG_WARN("set priority failed: {}", strerror(errno));
  }
#endif
#elif defined(__APPLE__)
  qos_class_t qosClass = QOS_CLASS_DEFAULT;
  switch (priority) {
    case Priority::kAudio:
      qosClass = QOS_CLASS_USER_INTERACTIVE;
      break;
    case Priority::kUrgentAudio:
      qosClass = QOS_CLASS_USER_INTERACTIVE;
      break;
    case Priority::kBackground:
      qosClass = QOS_CLASS_BACKGROUND;
      break;
    case Priority::kNormal:
      qosClass = QOS_CLASS_DEFAULT;
      break;
    case Priority::kDisplay:
      qosClass = QOS_CLASS_USER_INTERACTIVE;
      break;
    case Priority::kUrgentDisplay:
      qosClass = QOS_CLASS_USER_INTERACTIVE;
      break;
  }
  errno = 0;
  int error = pthread_set_qos_class_self_np(qosClass, 0);
#ifndef NDEBUG
  if (CORAL_UNLIKELY(error)) {
    MINILOG_WARN("pthread_set_qos_class_self_np failed: {}", strerror(errno));
  }
#endif
  if (priority == Priority::kAudio || priority == Priority::kUrgentAudio) {
    mach_port_t tid = pthread_mach_thread_np(pthread_self());
    thread_time_constraint_policy_data_t policy{};
    mach_timebase_info_data_t tb{};
    mach_timebase_info(&tb);
    double clock2abs = double(tb.denom) / double(tb.numer);

    uint32_t periodNs = 1'000'000; // 1ms, may dynamic calculate audio buffer.
    policy.period = (uint32_t) (periodNs * clock2abs);
    policy.computation = policy.period / 10;
    policy.constraint = policy.period / 2;
    policy.preemptible = 1;

    kern_return_t kr = thread_policy_set(tid, THREAD_TIME_CONSTRAINT_POLICY,
      (thread_policy_t) & policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
    if (kr != KERN_SUCCESS) {
      MINILOG_WARN("thread_policy_set failed: {}", mach_error_string(kr));
    }
  }
#elif defined(__linux__)
  struct sched_param param{};
  int policyType = SCHED_FIFO;
  param.sched_priority = (priority == Priority::kAudio || priority == Priority::kUrgentAudio) ? 70 : 0;
  int ret = pthread_setschedparam(pthread_self(), policyType, &param);
  if (ret != 0) {
    // Revert to the nice value
    int niceVal = (priority == Priority::kAudio || priority == Priority::kUrgentAudio) ? -16 : 0;
    setpriority(PRIO_PROCESS, 0, niceVal);
    MINILOG_WARN("SCHED_FIFO failed ({}), fell back to nice {}", strerror(ret), niceVal);
  }
#elif defined(WIN32)
  if (priority == Priority::kAudio || priority == Priority::kUrgentAudio) {
    // MMCSS is used by default.
    DWORD taskIndex = 0;
    HANDLE hMmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    if (hMmcss) {
      AvSetMmThreadPriority(hMmcss, AVRT_PRIORITY_CRITICAL);
    } else {
      // Revert to the THREAD_PRIORITY_TIME_CRITICAL priority
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }
  } else {
    // Revert to the nice value
    int winPriority = THREAD_PRIORITY_NORMAL;
    switch (priority) {
      case Priority::kBackground:
        winPriority = THREAD_PRIORITY_BELOW_NORMAL;
        break;
      case Priority::kNormal:
        winPriority = THREAD_PRIORITY_NORMAL;
        break;
      case Priority::kDisplay:
        winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
        break;
      default:
        winPriority = THREAD_PRIORITY_TIME_CRITICAL;
        break;
    }
    SetThreadPriority(GetCurrentThread(), winPriority);
  }
#endif
}

std::thread::id ThreadUtils::getThreadId() noexcept {
  return std::this_thread::get_id();
}

bool ThreadUtils::isThisThread(const std::thread::id id) noexcept {
  return getThreadId() == id;
}

void ThreadUtils::elevateCurrentThread() noexcept {
#if defined(__APPLE__)
  mach_port_t tid = pthread_mach_thread_np(pthread_self());
  thread_time_constraint_policy_data_t policy{};
  mach_timebase_info_data_t tb{};
  mach_timebase_info(&tb);
  const double ns = double(tb.numer) / double(tb.denom);
  policy.period = uint32_t(1_000_000 / ns); // 1ms
  policy.computation = uint32_t(500_000 / ns); // 0.5ms
  policy.constraint = uint32_t(1_500_000 / ns); // 1.5ms
  policy.preemptible = 1;
  thread_policy_set(tid, THREAD_TIME_CONSTRAINT_POLICY,
    (thread_policy_t) & policy,
    THREAD_TIME_CONSTRAINT_POLICY_COUNT);
#elif defined(__linux__)
  sched_param param{};
  param.sched_priority = 70; // needs CAP_SYS_NICE or root
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
#elif defined(_WIN32)
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
}

void ThreadUtils::sleepUntilSteady(std::chrono::steady_clock::time_point deadline) noexcept {
#if defined(__APPLE__)
  mach_timebase_info_data_t tb{};
  mach_timebase_info(&tb);
  const auto now = std::chrono::steady_clock::now();
  if (now < deadline) {
    const auto diff = deadline - now;
    const double ns = double(tb.denom) / double(tb.numer);
    const auto ticks = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() * ns);
    mach_wait_until(mach_absolute_time() + ticks);
  }
#elif defined(__linux__)
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline.time_since_epoch()).count();
  timespec ts{ time_t(ns / 1'000'000'000), long(ns % 1'000'000'000) };
  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
#else
  std::this_thread::sleep_until(deadline);
#endif
}

} // namespace coral
