/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

/**
 * @file test_JobSystem.cpp
 *
 * The work-stealing pool.
 *
 * A scheduler is the one component whose failures a functional test cannot see: a `parallelFor` that
 * silently ran half the range on one thread still produces the right answer, and a `wait` that returns
 * early still lets the test's next line pass. So the assertions here are about *how* the work ran as
 * much as about whether it ran -- every index exactly once, work observed on more than one thread,
 * and a pool of one worker completing a split it alone could run. Those three are what a broken
 * scheduler fails, and the first two are properties no single-threaded run can fake.
 */

#include <utils/JobSystem.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace {

using utils::JobSystem;

TEST(JobSystem, ReportsAtLeastTheCallingThread) {
  JobSystem pool(2, "test");
  EXPECT_GE(pool.workerCount(), 1u);
  EXPECT_EQ(pool.pendingJobCount(), 0u);
}

TEST(JobSystem, SubmitRunsTheJobAndReleasesTheCounter) {
  JobSystem pool(2, "test");

  std::atomic<int> ran { 0 };
  {
    auto counter = JobSystem::createCounter(0);
    for (int index = 0; index < 64; ++index) {
      pool.submit([&ran] { ran.fetch_add(1, std::memory_order_relaxed); }, *counter);
    }
    pool.wait(*counter);
  }

  EXPECT_EQ(ran.load(), 64);
  EXPECT_EQ(pool.pendingJobCount(), 0u);
}

TEST(JobSystem, WaitFromOutsideThePoolDoesNotReturnEarly) {
  // The failure this exists for: `wait` implemented as a check of the counter rather than as a real
  // rendezvous. Each job sleeps long enough that a premature return is a certainty, and the flag is
  // read only after the wait.
  JobSystem pool(3, "test");

  std::atomic<int> finished { 0 };
  {
    auto counter = JobSystem::createCounter(0);
    for (int index = 0; index < 8; ++index) {
      pool.submit([&finished] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        finished.fetch_add(1, std::memory_order_relaxed);
      }, *counter);
    }
    pool.wait(*counter);
  }

  EXPECT_EQ(finished.load(), 8);
}

TEST(JobSystem, ParallelForVisitsEveryIndexExactlyOnce) {
  JobSystem pool(0, "test");

  constexpr uint32_t kCount = 100000;
  std::vector<std::atomic<uint32_t>> visits(kCount);
  for (auto& visit: visits) visit.store(0, std::memory_order_relaxed);

  pool.parallelFor(kCount, 64, [&visits](const uint32_t begin, const uint32_t end) {
    for (uint32_t index = begin; index < end; ++index) {
      visits[index].fetch_add(1, std::memory_order_relaxed);
    }
  });

  for (uint32_t index = 0; index < kCount; ++index) {
    ASSERT_EQ(visits[index].load(std::memory_order_relaxed), 1u) << "index " << index;
  }
}

TEST(JobSystem, ParallelForRunsOnMoreThanOneThread) {
  // The property that separates a scheduler from a loop, and the one a sequential `parallelFor` would
  // still pass every other test in this file without: the set of threads that executed a range.
  //
  // Deliberately *not* a barrier. A barrier would make the test deadlock on a single-core machine
  // instead of failing it, and an assertion that hangs is not an assertion -- it is a build that never
  // finishes. Distinct thread ids are observed after the fact instead, which is a weaker statement that
  // is true exactly when the work was actually shared.
  //
  // **The task has to be real work, and that is not padding.** A task that only records a thread id
  // finishes in a few nanoseconds, so the calling thread claims the whole cursor before a worker has been
  // scheduled -- and the test then fails on a machine where sharing worked perfectly. The first version of
  // this test did exactly that and reported one thread on a twelve-core box. Each task now touches a
  // buffer, which both gives a worker time to arrive and checks the work was done.
  JobSystem pool(0, "test");

  constexpr uint32_t kCount = 200000;
  std::vector<std::atomic<uint32_t>> cells(kCount);
  for (auto& cell: cells) cell.store(0, std::memory_order_relaxed);

  std::mutex threadIdsMutex;
  std::set<std::thread::id> threadIds;

  pool.parallelFor(kCount, 32, [&cells, &threadIdsMutex, &threadIds](const uint32_t begin, const uint32_t end) {
    for (uint32_t index = begin; index < end; ++index) {
      cells[index].fetch_add(1, std::memory_order_relaxed);
    }
    std::lock_guard<std::mutex> lock(threadIdsMutex);
    threadIds.insert(std::this_thread::get_id());
  });

  for (uint32_t index = 0; index < kCount; ++index) {
    ASSERT_EQ(cells[index].load(std::memory_order_relaxed), 1u) << "index " << index;
  }

  // A pool created with 0 workers asks for one per core less the caller, so on any machine with more
  // than one core there is somebody to share with. A single-core machine legitimately reports one.
  const size_t expected = std::thread::hardware_concurrency() > 1 ? 2u : 1u;
  EXPECT_GE(threadIds.size(), expected)
          << "the split ran on " << threadIds.size() << " thread(s); a sequential implementation is indistinguishable "
             "from a correct one except here";
}

TEST(JobSystem, ParallelForOnASingleWorkerPoolCompletes) {
  // The degenerate case a recursive split deadlocks on when the waiting thread sleeps instead of
  // running the work it waits for. `workerCount` counts threads *besides* the caller, so a pool of 1
  // plus the caller is the smallest configuration that can still split -- and the caller is the only
  // thread that can run the branch it kept, which is the property under test.
  JobSystem pool(1, "test");

  std::atomic<uint32_t> visited { 0 };
  pool.parallelFor(4096, 16, [&visited](const uint32_t begin, const uint32_t end) {
    visited.fetch_add(end - begin, std::memory_order_relaxed);
  });

  EXPECT_EQ(visited.load(), 4096u);
}

TEST(JobSystem, NestedParallelForKeepsItsBoundaries) {
  JobSystem pool(0, "test");

  // The renderer's real shape: islands, each split again. The counters are per level, and a nested
  // `wait` must not consume a release that belongs to the outer one.
  constexpr uint32_t kOuter = 64;
  constexpr uint32_t kInner = 128;
  std::vector<std::atomic<uint32_t>> cells(kOuter * kInner);
  for (auto& cell: cells) cell.store(0, std::memory_order_relaxed);

  pool.parallelFor(kOuter, 4, [&pool, &cells](const uint32_t outerBegin, const uint32_t outerEnd) {
    for (uint32_t outer = outerBegin; outer < outerEnd; ++outer) {
      pool.parallelFor(kInner, 8, [&cells, outer](const uint32_t innerBegin, const uint32_t innerEnd) {
        for (uint32_t inner = innerBegin; inner < innerEnd; ++inner) {
          cells[outer * kInner + inner].fetch_add(1, std::memory_order_relaxed);
        }
      });
    }
  });

  for (size_t index = 0; index < cells.size(); ++index) {
    ASSERT_EQ(cells[index].load(std::memory_order_relaxed), 1u) << "cell " << index;
  }
}

TEST(JobSystem, ParallelForAsyncIsJoinedByOneWait) {
  JobSystem pool(0, "test");

  constexpr uint32_t kCount = 8192;
  std::vector<std::atomic<uint32_t>> hits(kCount);
  for (auto& hit: hits) hit.store(0, std::memory_order_relaxed);

  auto counter = JobSystem::createCounter(0);
  // `parallelForAsync` is called once and submits; the acquisition is the caller's, because the
  // function cannot know how many pieces the split will produce.
  JobSystem::acquire(*counter);
  pool.parallelForAsync(kCount, 32, *counter, [&hits](const uint32_t begin, const uint32_t end) {
    for (uint32_t index = begin; index < end; ++index) {
      hits[index].fetch_add(1, std::memory_order_relaxed);
    }
  });
  pool.wait(*counter);

  for (uint32_t index = 0; index < kCount; ++index) {
    ASSERT_EQ(hits[index].load(std::memory_order_relaxed), 1u) << "index " << index;
  }
}

TEST(JobSystem, ManySubmittersShareOneCounter) {
  // The counter has to accept acquisitions from threads that are not its creator, which is what a
  // frame-wide counter used by several recorders needs.
  JobSystem pool(4, "test");

  auto counter = JobSystem::createCounter(0);
  std::atomic<uint32_t> ran { 0 };

  std::vector<std::thread> submitters;
  for (int thread = 0; thread < 6; ++thread) {
    submitters.emplace_back([&pool, &counter, &ran] {
      for (int index = 0; index < 100; ++index) {
        pool.submit([&ran] { ran.fetch_add(1, std::memory_order_relaxed); }, *counter);
      }
    });
  }
  for (std::thread& thread: submitters) thread.join();

  pool.wait(*counter);
  EXPECT_EQ(ran.load(), 600u);
}

TEST(JobSystem, ZeroCountParallelForDoesNothing) {
  JobSystem pool(2, "test");
  bool ran = false;
  pool.parallelFor(0, 1, [&ran](uint32_t, uint32_t) { ran = true; });
  EXPECT_FALSE(ran);
}

} // namespace anonymous
