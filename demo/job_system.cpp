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
//  job_system.cpp
//
//  `utils::JobSystem` is a work-stealing thread pool. The demo
//  runs a parallel sum over a range with parallelFor(), then uses
//  the submit()/Counter pair for a batch of independent jobs, and
//  checks that the pool is empty afterwards.
//
//    coralcore_demo_job_system
// =============================================================

#include <utils/JobSystem.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>
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
  std::printf("CoralCore demo: job_system\n\n");

  // Four workers besides the calling thread, which also participates in the work.
  utils::JobSystem pool(4, "coralcore-demo");
  std::printf("  worker count: %u\n", pool.workerCount());
  check(pool.workerCount() >= 1, "JobSystem reports a usable worker count");

  // ------------------------------------------------------------------------------------------------
  // parallelFor: the range is cut into one piece per worker and the calling thread runs the last
  // piece, so the call returns once every index has been visited exactly once.
  // ------------------------------------------------------------------------------------------------
  constexpr uint32_t kCount = 200000;
  std::vector<uint32_t> values(kCount);
  for (uint32_t index = 0; index < kCount; ++index) {
    values[index] = index;
  }

  std::atomic<uint64_t> sum{ 0 };
  std::atomic<uint32_t> visited{ 0 };

  pool.parallelFor(kCount, 4096, [&values, &sum, &visited](uint32_t begin, uint32_t end) {
    uint64_t localSum = 0;
    uint32_t localVisited = 0;
    for (uint32_t index = begin; index < end; ++index) {
      localSum += values[index];
      ++localVisited;
    }
    sum.fetch_add(localSum, std::memory_order_relaxed);
    visited.fetch_add(localVisited, std::memory_order_relaxed);
  });

  const uint64_t expectedSum = static_cast<uint64_t>(kCount - 1) * kCount / 2;
  std::printf("  parallelFor visited %u of %u indices, sum %llu of %llu\n", visited.load(std::memory_order_relaxed),
          kCount, static_cast<unsigned long long>(sum.load(std::memory_order_relaxed)),
          static_cast<unsigned long long>(expectedSum));

  check(visited.load(std::memory_order_relaxed) == kCount, "parallelFor() visited every index exactly once");
  check(sum.load(std::memory_order_relaxed) == expectedSum, "parallelFor() computed the right sum");

  // ------------------------------------------------------------------------------------------------
  // submit + Counter: the caller acquires the counter once per job (submit does it here) and waits
  // once, at the end, instead of blocking on each job separately.
  // ------------------------------------------------------------------------------------------------
  constexpr int kJobs = 32;
  std::atomic<int> completed{ 0 };

  {
    const std::unique_ptr<utils::JobSystem::Counter> counter = utils::JobSystem::createCounter(0);
    for (int job = 0; job < kJobs; ++job) {
      pool.submit([&completed] { completed.fetch_add(1, std::memory_order_relaxed); }, *counter);
    }
    pool.wait(*counter);
  }

  check(completed.load(std::memory_order_relaxed) == kJobs, "submit() ran every job before wait() returned");
  check(pool.pendingJobCount() == 0, "the pool is idle after the callback batch");

  std::printf("\n%d checks, %d failures\n", gChecks, gFailures);
  std::printf("%s\n", gFailures == 0 ? "coralcore_demo_job_system: PASS" : "coralcore_demo_job_system: FAIL");
  return gFailures == 0 ? 0 : 1;
}
