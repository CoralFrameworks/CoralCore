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

#ifndef CORAL_UTILS_JOB_SYSTEM_H
#define CORAL_UTILS_JOB_SYSTEM_H

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace utils {

/**
 * A work-stealing thread pool.
 *
 * ## Why this exists rather than `std::async`
 *
 * The 2D renderer's parallelism is *nested and recursive*: an island of a draw list is split into
 * sub-islands, each of which is split again, and a parent must not finish before its children have.
 * `std::async` gives a future per call and no way to run a child on the calling thread, so a nested
 * split spawns a thread per node and a 10 000-triangle path becomes 10 000 threads. This pool is a
 * fixed set of workers with a deque per worker, so nesting costs a push and a pop rather than a
 * thread.
 *
 * ## The model
 *
 * Modelled on Filament's `JobSystem`, which is itself the standard Cilk-style design:
 *
 *  - **Every worker owns a deque.** A worker pushes and pops at the *back*, which is the hot path and
 *    needs no synchronisation with itself.
 *  - **Stealing is from the front**, which is the coldest end -- the oldest work -- so a thief and the
 *    owner rarely touch the same cache line.
 *  - **A waiting task runs work instead of sleeping.** `wait()` on a counter does not block the thread
 *    until it has drained its own queue; it steals and executes, because a worker blocked on a
 *    counter is a worker that cannot help finish the job it is waiting for. This is the property that
 *    keeps a recursive split from deadlocking on a single-core machine, where the child will never be
 *    scheduled if the parent sleeps.
 *  - **The submitting thread participates.** The thread that calls `submit` pushes to its own deque
 *    and only becomes a worker for the duration of the call, so a caller that never touches the pool
 *    again does not hold a worker hostage.
 *
 * ## What it is not
 *
 * Not a general-purpose coroutine scheduler: a job runs to completion on one thread, there is no
 * yield, and there is no cancellation. Both would be features without a consumer here -- the renderer
 * splits work it can finish in microseconds and never wants to cancel half of it.
 *
 * ## Lifetime
 *
 * RAII throughout: the destructor drains every deque and joins every thread, so a `JobSystem` on the
 * stack is joined before it goes out of scope. `wait()` must still be called for any counter the
 * caller owns, because a job that is still queued when the pool is destroyed will not have run and
 * whatever it was going to fill in will not have been filled in -- the destructor joining threads is
 * not the same promise as the caller's own join.
 */
class JobSystem {
public:
  /**
   * A completion counter: acquired once per outstanding job and released by each as it finishes.
   *
   * ## Why this is a complete type in the header
   *
   * `createCounter` hands one out through a `unique_ptr`, so the deleter has to see the destructor, and
   * a forward-declared nested class is an *incomplete* type at that point: the failure is a
   * `static_assert` inside `<memory>` reading "can't delete an incomplete type", which names neither
   * this class nor the reason. Declaring it here costs a `<mutex>` and a `<condition_variable>` in the
   * header and buys a diagnostic that makes sense, which is the right side of that trade.
   *
   * ## What the members are for
   *
   * The count is atomic because children release the *same* counter concurrently. `mMutex`, `mParked`
   * and `mCondition` cover the one case the atomic cannot: a thread that is not a pool worker must not
   * spin, so it has to park, and the thread that drives the count to zero has to wake it.
   */
  class Counter {
  public:
    Counter() = default;

    Counter(const Counter&) = delete;
    Counter& operator=(const Counter&) = delete;

    /** Adds @p count outstanding jobs. */
    void acquire(uint32_t count) noexcept {
      if (count == 0) return;
      mCount.fetch_add(count, std::memory_order_relaxed);
    }

    /** Marks one job finished, waking a parked waiter when this was the last. */
    void release() noexcept;

    /** @return true when every acquisition has been released. */
    [[nodiscard]] bool done() const noexcept { return mCount.load(std::memory_order_acquire) == 0; }

    /** @return how many jobs are still outstanding. */
    [[nodiscard]] uint64_t count() const noexcept { return mCount.load(std::memory_order_relaxed); }

    /** Parks until the counter reaches zero. Only a non-worker may call this. */
    void park();

  private:
    std::atomic<uint64_t> mCount { 0 };

    std::mutex mMutex;
    std::condition_variable mCondition;
    bool mParked = false;
  };

  /**
   * A job: a plain callable invoked once. It must not throw; a throw propagates out of a worker
   * thread and terminates the process, so a job that can fail should report by return value.
   */
  using Job = std::function<void()>;

  /**
   * @param workerCount number of worker threads *besides* the calling thread. 0 asks for the hardware
   *        concurrency minus one, so that a machine with N cores ends up with N runnable threads
   *        including the caller.
   * @param name a prefix for the worker threads, so a profiler or a debugger can name them.
   */
  explicit JobSystem(uint32_t workerCount = 0, const char* name = "coral-gpu");
  ~JobSystem();

  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;
  JobSystem(JobSystem&&) = delete;
  JobSystem& operator=(JobSystem&&) = delete;

  /** @return the thread pool size, counting the submitting thread when it participates. */
  [[nodiscard]] uint32_t workerCount() const noexcept { return mWorkerCount; }

  /** @return the number of jobs submitted and not yet completed. Diagnostics only. */
  [[nodiscard]] uint64_t pendingJobCount() const noexcept;

  /** @return true when this call is running on a pool worker. */
  [[nodiscard]] static bool isWorkerThread() noexcept;

  // ------------------------------------------------------------------------------------------------
  // Submission
  // ------------------------------------------------------------------------------------------------

  /**
   * Runs @p job on some worker, acquiring @p counter for it and releasing the counter when it
   * finishes.
   *
   * The acquisition is `submit`'s because this is the only place that can order it correctly:
   * acquiring after the push would let a fast worker release a counter that is still at zero and
   * drive it negative, so the waiter would block forever on a counter that can never be satisfied.
   * A caller that does not know the submission count up front creates a counter at zero
   * (`createCounter(0)`) and waits on it once, at the end.
   *
   * The job is pushed onto the calling thread's deque when the caller is a worker, which makes a
   * nested split run depth-first on the thread that made it and leaves exactly the tail for a thief.
   * From outside the pool it goes to a worker chosen round robin, so no single worker's queue grows
   * under a stream of submissions.
   */
  void submit(const Job& job, Counter& counter);

  /** @return a counter acquired @p count times, to be passed to @p count `submit` calls. */
  [[nodiscard]] static std::unique_ptr<Counter> createCounter(uint32_t count);

  /**
   * Acquires @p counter @p count times without allocating.
   *
   * The form a renderer uses: one counter per frame, reused, re-acquired once per split, so that a
   * frame does not allocate once per island.
   */
  static void acquire(Counter& counter, uint32_t count = 1) noexcept;

  /**
   * Blocks until every acquisition of @p counter has been released.
   *
   * **Blocks is the wrong verb when called from a worker.** There the call drains stolen jobs until
   * the counter reaches zero, so a parent waits for its children by helping to finish them. On a
   * non-worker thread it runs whatever the pool has queued and then parks on a condition variable --
   * with the outstanding job still able to run on any worker, which is why this is not a spin.
   *
   * A member rather than a static because a waiter has real work to do: it drains the pool the counter
   * belongs to. The test suite's `JobSystem::wait(*counter)` form works because the member can be
   * called on any live pool, and a counter is only ever waited on by a thread that has one.
   */
  void wait(Counter& counter);

  /** Waits for @p counter and returns it to the pool for reuse. */
  void waitAndReset(Counter& counter) noexcept;

  /**
   * Runs @p range over `[0, count)` in parallel and waits for all of it.
   *
   * The call a renderer actually makes: "tessellate these 4096 paths". The range is cut into
   * `min(workerCount(), count / grainSize)` **contiguous** pieces; the last piece runs on the calling thread and the rest
   * are submitted, then waited on in submission order.
   *
   * ## The piece boundaries are static, so the caller's order decides the balance
   *
   * Nothing here can move work from a busy piece to an idle one: a piece is a fixed index range, computed before any of
   * it runs. That is a deliberate trade -- the split costs nothing and there is no shared cursor to contend for -- but it
   * puts the load balance in the *caller's* hands, and a caller whose items differ in cost has to arrange them, which is
   * what `balanceByCost` below is for. Measured on the graphics side, where a stage went from 30.5 ms to 12 ms by
   * changing only the order of the items it submitted (路径边缘抗锯齿设计与现状.md 4.1.22).
   *
   * The calling thread takes part: it runs one piece, so a pool of one worker still completes the call rather than
   * deadlocking on work it was the only thread able to run.
   *
   * @param count     number of indices in `[0, count)`.
   * @param grainSize smallest number of indices one piece covers; 0 means "one per worker".
   * @param range     invoked as `range(begin, end)` on one thread. Must not throw.
   */
  void parallelFor(uint32_t count, uint32_t grainSize, const std::function<void(uint32_t, uint32_t)>& range);

  /**
   * Permutes @p items so that a following `parallelFor(items.size(), 1, range)` splits them into pieces of even **cost**.
   *
   * @param items permuted in place. The permutation is a permutation: every item is still there exactly once, so a caller
   *              whose work is order-independent -- which every caller of `parallelFor` must be, since it promises no
   *              ordering -- gets the same result in the same time, only spread out.
   * @param cost  invoked per item and returning what one item costs; called `O(n log n)` times by the sort, so it should
   *              be cheap (reading a field rather than measuring anything).
   * @return the number of pieces `parallelFor` will use for @p items, which is what a caller needs to reason about the
   *         balance; 1 when the array is too small or the pool has one thread, in which case nothing is permuted.
   *
   * ## Why this is the pool's business rather than each caller's
   *
   * `parallelFor` cuts a *contiguous* index range, so balancing means choosing an order: sorting by cost alone does not do
   * it -- the heaviest items then all land in the first piece -- and what does is dealing the sorted items out
   * round-robin, so piece `c` takes `cost[c]`, `cost[c + pieces]`, `cost[c + 2 * pieces]`, and every piece gets one item
   * from each rank of the sorted order. Getting that right is not obvious, it is the same reasoning at every call site,
   * and getting it *wrong* looks exactly like a pool that does not work: the graphics coverage route spent four rounds
   * measuring allocations, write patterns and rasteriser internals before the load balance itself was measured at 10.9x
   * the even share (4.1.22).
   */
  template<typename Item, typename Cost>
  uint32_t balanceByCost(std::vector<Item>& items, Cost&& cost) const {
    if (items.size() <= 1 || mWorkerCount <= 1) return 1;
    const uint32_t pieces = std::max(1u, std::min<uint32_t>(mWorkerCount, static_cast<uint32_t>(items.size())));
    if (pieces <= 1) return 1;

    std::stable_sort(items.begin(), items.end(), [&cost](const Item& left, const Item& right) {
      return cost(left) > cost(right);
    });

    // Built piece by piece rather than by writing into a computed index: `parallelFor` gives each piece
    // `ceil(count / pieces)` items and a shorter last one, so a single index formula for the whole array would write past
    // the end whenever the count is not a multiple of the piece count.
    std::vector<Item> dealt;
    dealt.reserve(items.size());
    for (uint32_t piece = 0; piece < pieces; ++piece) {
      for (size_t index = piece; index < items.size(); index += pieces) {
        dealt.push_back(std::move(items[index]));
      }
    }
    items.swap(dealt);
    return pieces;
  }

  /**
   * The same split as `parallelFor`, but it hands the work to the pool and releases @p counter as it goes.
   *
   * The range is cut into one piece per worker, every piece but one is submitted, and the last runs on the
   * calling thread. The caller waits once, at the end, on @p counter -- which is the point of the call: a
   * renderer that dispatches several independent lists and then waits for all of them together instead of
   * serialising on one list at a time.
   *
   * **It may return later than "after queueing".** The work is distributed and then awaited, so the counter
   * reaches zero when the range is done rather than when it was queued. See the implementation for the two
   * designs that tried to return sooner and deadlocked: a job that runs a split has to wait for the pieces
   * it spawns, and a pool whose every worker is inside such a wait has no thread left to run the pieces.
   *
   * @param counter acquired by the caller once per call, *not* once per piece.
   */
  void parallelForAsync(uint32_t count, uint32_t grainSize, Counter& counter,
          const std::function<void(uint32_t, uint32_t)>& range);

private:
  /**
   * Runs queued jobs until @p counter reaches zero.
   *
   * The body of `wait`'s worker case: a worker that is waiting for work it dispatched runs it instead of
   * sleeping, which is what keeps a pool of one worker from deadlocking against itself.
   */
  void drainUntilDone(Counter& counter);

  class Impl;
  std::unique_ptr<Impl> mImpl;

  /**
   * Number of threads that can execute a job at once, counting the submitting thread. Stored here
   * rather than asked of the implementation because `parallelFor`'s grain size is derived from it and
   * that is on the hot path.
   */
  uint32_t mWorkerCount = 1;
};

} // namespace utils

#endif // CORAL_UTILS_JOB_SYSTEM_H

