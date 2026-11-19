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

// =============================================================
//  hello.cpp
//
//  The smallest useful CoralCore program. It links `core` only and
//  uses the three things a caller needs first: the monotonic
//  SystemClock, a Stopwatch around a sleep, and a CountDownLatch
//  that four threads count down.
//
//    coralcore_demo_hello
//
//  Exits with 0 when every check passed, 1 otherwise, so it can be
//  used as a build/run smoke test.
// =============================================================

#include <core/task/CountDownLatch.h>
#include <core/time/Stopwatch.h>
#include <core/time/SystemClock.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

namespace {

int gChecks = 0;
int gFailures = 0;

void check(const bool condition, const char* what) {
  ++gChecks;
  std::printf("  %s %s\n", condition ? "ok  " : "FAIL", what);
  if (!condition) {
    ++gFailures;
  }
}

} // namespace

int main() {
  std::printf("CoralCore demo: hello\n\n");

  // SystemClock is monotonic: it measures elapsed time, not the wall clock.
  const int64_t clockBefore = coral::SystemClock::CurrentTimeMillis();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  const int64_t clockAfter = coral::SystemClock::CurrentTimeMillis();
  std::printf("  SystemClock advanced %lld ms\n", static_cast<long long>(clockAfter - clockBefore));
  check(clockAfter >= clockBefore, "SystemClock::CurrentTimeMillis() moves forward");

  // Stopwatch is created by the platform, so the demo links only the public header.
  const std::unique_ptr<coral::Stopwatch> stopwatch = coral::Stopwatch::Create();
  check(stopwatch != nullptr, "Stopwatch::Create() returns a platform stopwatch");
  if (stopwatch == nullptr) {
    std::printf("\ncoralcore_demo_hello: FAIL\n");
    return 1;
  }

  stopwatch->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  stopwatch->stop();
  const double elapsedSeconds = stopwatch->getElapsedTime();
  std::printf("  Stopwatch measured %.1f ms\n", elapsedSeconds * 1000.0);
  check(elapsedSeconds >= 0.010, "the Stopwatch measured at least the requested sleep");

  // A latch is the cross-thread join primitive in `core/task`: the main thread waits here while the
  // workers sleep for different amounts and call latch().
  constexpr int kWorkers = 4;
  coral::CountDownLatch latch(kWorkers);

  std::vector<std::thread> workers;
  workers.reserve(kWorkers);
  for (int index = 0; index < kWorkers; ++index) {
    workers.emplace_back([&latch, index] {
      std::this_thread::sleep_for(std::chrono::milliseconds(5 * (index + 1)));
      latch.latch();
    });
  }

  latch.await();
  for (std::thread& worker: workers) {
    worker.join();
  }

  std::printf("  latch counted down %zu times\n", latch.getCount());
  check(latch.getCount() == static_cast<size_t>(kWorkers), "every worker counted the latch down");

  std::printf("\n%d checks, %d failures\n", gChecks, gFailures);
  std::printf("%s\n", gFailures == 0 ? "coralcore_demo_hello: PASS" : "coralcore_demo_hello: FAIL");
  return gFailures == 0 ? 0 : 1;
}
