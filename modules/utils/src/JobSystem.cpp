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

#include <utils/JobSystem.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace utils {

// ==================================================================================================
// Thread identity
// ==================================================================================================
//
// A thread_local raw pointer to the slot a worker owns. `isWorkerThread` and `submit`'s decision to push
// onto "my own" queue both read it, and both are on the hot path, so it has to be a thread_local rather
// than a lookup in a map keyed by a thread id.
//
// It is `void*` rather than a typed pointer because the slot type is private to the implementation and
// this variable has to exist before that type is declared. The only reader that needs the type is the
// implementation, one cast away.

namespace {

thread_local void* tCurrentWorker = nullptr;

} // namespace anonymous

// ==================================================================================================
// Counter
// ==================================================================================================
//
// The class is declared in the header because its destructor has to be visible to the `unique_ptr` that
// `createCounter` hands out. The two members that wait on it are defined here, where `<mutex>` and
// `<condition_variable>` are already included for the pool's own use.
//
// ## Why each counter waits on its own condition variable
//
// A counter owns the mutex and the condition variable it is waited on through. The alternative -- one
// condition variable for the whole process, notified whenever any counter reaches zero -- is simpler to
// write and makes every wait a potential thundering herd: a frame with a thousand counters wakes all of
// its waiters on each release, and each re-checks a predicate that has not changed. Per counter, a release
// wakes exactly the threads waiting for *that* counter.
//
// The protocol is the standard one and both halves matter: `release` takes the mutex before notifying, so
// a waiter that has already committed to waiting is on the condition variable's list; and `park` re-tests
// the predicate under the same mutex, so a release that landed before the wait began is seen rather than
// missed.

void JobSystem::Counter::release() noexcept {
  const uint64_t previous = mCount.fetch_sub(1, std::memory_order_acq_rel);
  if (previous > 1) return;

  if (previous == 0) {
    // A release with nothing outstanding, which means the caller's accounting is wrong. Ignoring it is far
    // worse than reporting it: the count wraps to `UINT64_MAX`, a waiter on it never returns, and the
    // symptom is a hang in code that has nothing to do with the real mistake. Put the count back so the
    // counter stays usable, and leave the wrongness where a debugger can see it.
    mCount.fetch_add(1, std::memory_order_relaxed);
    return;
  }

  // The last release. Nothing else will run and produce another, so the waiter -- if there is one yet --
  // has to be told. Held under the mutex so the notification cannot fall into the gap between a waiter's
  // predicate test and its first wait.
  std::lock_guard<std::mutex> lock(mMutex);
  mCondition.notify_all();
}

void JobSystem::Counter::park() {
  // ## Why this spins before it sleeps
  //
  // A condition variable is the right way to wait for work that may take milliseconds, and it is the wrong
  // way to wait for work that may take microseconds: the syscall costs more than the wait, and every
  // protocol detail -- the predicate, the lock ordering, the notification -- is a place to lose a wakeup.
  // The waits this class actually serves are the former far more often than the latter: a `parallelFor`
  // over a few thousand tasks finishes in tens of microseconds, and its caller should still be running the
  // task loop when it does.
  //
  // So the fast path is a bounded spin: `done()` is one acquire load, and a spin that finds the work already
  // finished costs nanoseconds. Only after `kSpinRounds` yields has the wait become long enough that
  // sleeping beats spinning, and only then is the condition variable entered -- under the mutex, with the
  // predicate re-tested, so a release that landed during the spin is seen rather than missed.
  //
  // The scheme is therefore correct even if the notification is lost entirely: the worst case is
  // `kSpinRounds` yields and then a `wait_for` timeout, not a hang.
  constexpr int kSpinRounds = 4096;
  for (int round = 0; round < kSpinRounds; ++round) {
    if (done()) return;
    std::this_thread::yield();
  }

  std::unique_lock<std::mutex> lock(mMutex);
  while (!done()) {
    // A bounded sleep rather than an unbounded wait, for the same reason: a notification that is somehow
    // missed costs one timeout, and the loop re-tests. Two milliseconds is far below the threshold at which
    // a frame budget is at risk and far above the cost of checking an atomic.
    mCondition.wait_for(lock, std::chrono::milliseconds(2));
  }
}

// ==================================================================================================
// The pool
// ==================================================================================================

class JobSystem::Impl {
public:
  /**
   * One slot per worker: the queue it drains, the lock that guards it, and the thread that owns it.
   *
   * The queue is a `std::deque` and every operation on it takes @c mutex, including the owner's own pop.
   * The container choice still matters -- a thief takes from the front while the owner takes from the
   * back, so a `vector` would relocate elements under a concurrent `pop_front` -- but "the owner's end
   * needs no lock" would be wrong, because `push` and `steal` both mutate this deque under the lock and
   * an unlocked owner pop would race them on the container itself rather than on a size field.
   */
  struct Worker {
    std::deque<JobSystem::Job> queue;
    /** Published length of @c queue, so a thief can skip a slot that is plainly empty. */
    std::atomic<uint32_t> size { 0 };
    /** Set while the worker is idle and waiting. Only ever written while holding @c mutex. */
    bool idle = false;
    std::mutex mutex;
    std::condition_variable condition;
    std::thread thread;
    uint32_t index = 0;
  };

  Impl(const uint32_t requestedWorkers, const char* name) {
    const uint32_t hardware = std::max(1u, std::thread::hardware_concurrency());
    // `workerCount` counts threads *besides* the caller, and the caller still participates, so asking for
    // 0 means "one per core, less the caller": a machine with N cores then has N runnable threads.
    const uint32_t workers = requestedWorkers != 0 ? requestedWorkers : (hardware > 1 ? hardware - 1 : 0);
    mName = name != nullptr ? name : "coral";

    // A `std::deque` of slots rather than a `std::vector`, and not for the reason a queue is a deque: a
    // slot owns a `std::mutex` and a `std::condition_variable`, so it is neither copyable nor movable, and
    // a `vector` refuses to grow a type it cannot relocate. A deque constructs in place and never moves
    // what it already holds -- which is also what keeps `&mSlots[i]` stable for a worker that cached its
    // own slot.
    for (uint32_t index = 0; index < workers; ++index) {
      mSlots.emplace_back();
      mSlots.back().index = index;
    }
    for (uint32_t index = 0; index < workers; ++index) {
      mSlots[index].thread = std::thread([this, index] { workerLoop(index); });
    }
  }

  ~Impl() {
    // The stop flag is published before any worker is told to look, and every wake-up path re-reads it, so
    // a worker cannot observe "not stopping" and then sleep forever: the join below would have hung, and
    // that is the failure this ordering exists to rule out.
    mStopping.store(true, std::memory_order_seq_cst);
    for (uint32_t index = 0; index < mSlots.size(); ++index) {
      wake(index);
    }
    for (Worker& worker: mSlots) {
      if (worker.thread.joinable()) worker.thread.join();
    }
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  /** @return the slot for the calling thread, or nullptr when the caller is not a pool worker. */
  [[nodiscard]] Worker* currentWorker() const noexcept { return static_cast<Worker*>(tCurrentWorker); }

  /** The slot a non-worker submits to: round robin, so no single worker's queue grows without bound. */
  [[nodiscard]] Worker& pickQueue() noexcept {
    const uint32_t next = mRoundRobin.fetch_add(1, std::memory_order_relaxed);
    return mSlots[next % mSlots.size()];
  }

  void push(Worker& worker, const Job& job) {
    {
      std::lock_guard<std::mutex> lock(worker.mutex);
      worker.queue.push_back(job);
      worker.size.store(static_cast<uint32_t>(worker.queue.size()), std::memory_order_release);
    }
    wake(worker.index);
  }

  /**
   * Takes from the owner's end.
   *
   * The lock is not optional: `push` and `steal` both mutate this deque under it, so an owner that popped
   * without it would race them on the container itself. That is not a benign race on a size field -- it is
   * `pop_back` racing `pop_front`, which the standard library detects as "pop_back() called on empty
   * deque" in a debug build and which silently corrupts the internal map in a release one. One
   * uncontended lock per job is the whole price of a thief being possible.
   */
  [[nodiscard]] bool popOwn(Worker& worker, Job* outJob) {
    std::lock_guard<std::mutex> lock(worker.mutex);
    if (worker.queue.empty()) return false;
    *outJob = std::move(worker.queue.back());
    worker.queue.pop_back();
    worker.size.store(static_cast<uint32_t>(worker.queue.size()), std::memory_order_release);
    return true;
  }

  /** Steals the oldest job from @p victim's queue: the end the owner will not reach for a long time. */
  [[nodiscard]] bool steal(Worker& victim, Job* outJob) {
    std::lock_guard<std::mutex> lock(victim.mutex);
    if (victim.queue.empty()) return false;
    *outJob = std::move(victim.queue.front());
    victim.queue.pop_front();
    victim.size.store(static_cast<uint32_t>(victim.queue.size()), std::memory_order_release);
    return true;
  }

  /** Tries every other slot once. @return false when the pool is dry. */
  [[nodiscard]] bool stealAny(Worker& self, Job* outJob) {
    const uint32_t count = static_cast<uint32_t>(mSlots.size());
    // Started one past the caller's own slot so two idle workers do not both begin at slot 0 and collide
    // on the same victim every time.
    for (uint32_t offset = 1; offset < count; ++offset) {
      Worker& victim = mSlots[(self.index + offset) % count];
      if (victim.size.load(std::memory_order_acquire) == 0) continue;
      if (steal(victim, outJob)) return true;
    }
    return false;
  }

  /**
   * Wakes @p index if it is idle.
   *
   * The check and the notify happen under the victim's mutex, and that is not a convenience: the worker
   * sets `idle` and then re-checks its queue under the same mutex, so either this sees `idle == true` and
   * notifies a thread that is (or is about to be) waiting, or it sees `false` and the worker is about to
   * look at the queue this push has already filled. A check outside the lock would leave a window in which
   * the worker sleeps after the check and is never woken.
   */
  void wake(const uint32_t index) {
    Worker& worker = mSlots[index];
    std::lock_guard<std::mutex> lock(worker.mutex);
    if (!worker.idle) return;
    worker.condition.notify_one();
  }

  /**
   * The worker body: drain, steal, then wait.
   *
   * The wait carries a one-millisecond timeout rather than trusting a notification alone. That is
   * deliberate belt and braces: the notification protocol is correct, but a missed one would be a hang
   * rather than a slowdown, and the timeout turns the worst case into a millisecond of latency that no
   * interactive workload can perceive. A genuinely idle worker wakes sixty times between two frames of a
   * 60 Hz display, finds nothing, and costs nothing measurable.
   */
  void workerLoop(const uint32_t index) {
    Worker& self = mSlots[index];
    tCurrentWorker = &self;

    // The thread-local attachment covers the whole body, including the teardown drain, and is cleared on
    // every exit path. Both halves matter and each was learned from a failure: a job that is still running
    // when the pool stops is *still a job*, so a nested `wait` inside it has to find a worker and help
    // drain rather than park on a counter nothing will release; and a stale pointer to a destroyed pool's
    // slot makes `submit` push onto a freed deque, which is how a second pool in one process hung.
    struct AttachmentGuard {
      ~AttachmentGuard() { tCurrentWorker = nullptr; }
    } guard;

    while (true) {
      Job job;
      if (popOwn(self, &job) || stealAny(self, &job)) {
        runJob(job);
        continue;
      }

      if (mStopping.load(std::memory_order_seq_cst)) break;

      std::unique_lock<std::mutex> lock(self.mutex);
      self.idle = true;
      self.condition.wait_for(lock, std::chrono::milliseconds(1), [this, &self] {
        return mStopping.load(std::memory_order_seq_cst) || !self.queue.empty();
      });
      self.idle = false;
    }

    // Teardown: run whatever is left, so that a job submitted just before destruction is not silently
    // dropped. The header still asks the caller to wait for its own counters -- this makes the *pool's*
    // lifetime safe, which is a different promise from the caller's own join being redundant.
    Job job;
    while (popOwn(self, &job) || stealAny(self, &job)) {
      runJob(job);
    }
  }

  void runJob(Job& job) {
    job();
    mPending.fetch_sub(1, std::memory_order_relaxed);
  }

  std::deque<Worker> mSlots;
  std::string mName;
  std::atomic<uint32_t> mRoundRobin { 0 };
  std::atomic<bool> mStopping { false };
  std::atomic<uint64_t> mPending { 0 };
};

// ==================================================================================================
// JobSystem
// ==================================================================================================

JobSystem::JobSystem(const uint32_t workerCount, const char* name)
  : mImpl(std::make_unique<Impl>(workerCount, name)) {
  // The submitting thread counts as one runnable thread, so a pool with N workers executes N+1 jobs at
  // once -- which is what the range split assumes when it keeps one share for the caller.
  mWorkerCount = static_cast<uint32_t>(mImpl->mSlots.size()) + 1;
}

JobSystem::~JobSystem() = default;

bool JobSystem::isWorkerThread() noexcept { return tCurrentWorker != nullptr; }

uint64_t JobSystem::pendingJobCount() const noexcept { return mImpl->mPending.load(std::memory_order_relaxed); }

void JobSystem::acquire(Counter& counter, const uint32_t count) noexcept { counter.acquire(count); }

std::unique_ptr<JobSystem::Counter> JobSystem::createCounter(const uint32_t count) {
  auto counter = std::make_unique<Counter>();
  counter->acquire(count);
  return counter;
}

void JobSystem::submit(const Job& job, Counter& counter) {
  // A job submitted from a worker goes on that worker's own queue, which it drains before it steals: a
  // nested dispatch therefore runs depth-first on the thread that made it, and the jobs it does not run
  // itself are exactly the ones a thief can pick up. From outside the pool the job has to go to a worker's
  // queue, because the submitting thread is about to block on the counter.
  Impl::Worker* self = mImpl->currentWorker();
  Impl::Worker& queue = self != nullptr ? *self : mImpl->pickQueue();

  // The count is raised *before* the job can be seen by anyone, which is the ordering the whole wait
  // depends on: a fast worker that released a counter still at zero would drive it negative, and the
  // waiter would then block forever on a counter that can never be satisfied.
  mImpl->mPending.fetch_add(1, std::memory_order_relaxed);
  counter.acquire(1);

  Counter& released = counter;
  mImpl->push(queue, [&released, job] {
    job();
    released.release();
  });
}

void JobSystem::wait(Counter& counter) {
  if (counter.done()) return;

  // A worker waiting for work it dispatched has to *run* it: on a one-worker pool the dispatcher is the
  // only thread that can execute a job it just queued, so a worker that slept would deadlock against
  // itself. A thread that is not a worker owns no slot and cannot steal, so it parks -- and the last
  // release wakes it.
  if (mImpl->currentWorker() != nullptr) {
    drainUntilDone(counter);
    return;
  }

  counter.park();

  // `runJob` decrements the pending count *after* the job body returns -- deliberately, so that the count
  // means "jobs still executing" -- and a job body releases its counter before returning. A caller that
  // observed the counter at zero could therefore still see one job counted as pending, which is a
  // momentarily-true number that a test asserting "nothing is left" would read as a leak. Waiting for it
  // here makes the two agree at the moment `wait` returns, which is the promise a caller actually wants.
  //
  // **Bounded, and it has to be.** A caller that waits on counter A can be woken by an unrelated job
  // releasing counter B, and that other job may still be running for a while. Spinning until the *pool* is
  // empty would then be a wait for work this caller has nothing to do with. The count reaching zero is the
  // real signal and it has already arrived; this only gives a job that released a moment ago the chance to
  // finish bookkeeping.
  for (int spin = 0; spin < 64 && mImpl->mPending.load(std::memory_order_acquire) != 0; ++spin) {
    std::this_thread::yield();
  }
}

void JobSystem::waitAndReset(Counter& counter) noexcept {
  wait(counter);
  // Nothing to clear beyond the count, which is already zero. The method exists so the call site reads as
  // "this counter is settled and may be reused", which is the contract `acquire` relies on.
}

void JobSystem::drainUntilDone(Counter& counter) {
  while (!counter.done()) {
    Impl::Worker* self = mImpl->currentWorker();
    if (self == nullptr) return;

    Job job;
    if (mImpl->popOwn(*self, &job) || mImpl->stealAny(*self, &job)) {
      mImpl->runJob(job);
      continue;
    }

    // Every queue is empty and the counter is not zero, so the outstanding work is already running on
    // another thread. Yielding is the honest answer: spinning hard would take time from exactly the threads
    // that have to finish before this loop can end.
    std::this_thread::yield();
  }
}

// ==================================================================================================
// Range splitting

// ==================================================================================================
// Range splitting
// ==================================================================================================

// ==================================================================================================
// Range splitting
// ==================================================================================================
//
// ## A flat split, and the three designs that were tried before it
//
// Parallelising a range looks like it has one obvious answer and it does not. Each of these was implemented
// and each failed in a way worth recording, because the failures are what the current shape is shaped around:
//
//  1. **Recursive halving with a counter per split, one counter for both halves.** Correct for a flat range
//     and wrong under nesting: a `range` that itself calls `parallelFor` releases the *outer* counter from
//     the inner dispatch, so the outer count reaches zero while the outer work is still running and the outer
//     caller overwrites buffers the inner work is still writing. Wrong results rather than a crash.
//  2. **A shared atomic cursor with one counter per participant.** Correct for nesting and for the counts, and
//     it produced a queue that grew while every worker spun: a participant that claimed and ran a *nested*
//     `parallelFor` blocked inside it waiting for work that only it could have run.
//  3. **Recursive halving with a counter per submitted job.** Correct for nesting, and it got the counts
//     right -- and it starved the pool, because every submitted job spawned more submitted jobs and then
//     blocked waiting for them. With eleven workers and eleven jobs in flight, no thread was left to run the
//     grandchildren. It also overflowed the stack on a 100 000 element range in a debug build.
//
// What replaced all three is the simplest shape that does not spawn from a spawned job: **one level of
// split**. The range is cut into at most `mWorkerCount` contiguous pieces. One piece runs on the calling
// thread; the others are submitted, and each submitted job runs its whole piece with no further submission.
// It is worth stating what that gives up: no load balancing, so a piece that is forty times the work of
// another finishes last and the tail of the call is one thread busy and eleven idle. For the work this pool
// actually serves -- tessellating islands of similar size, sorting similar batches -- that is close to no
// loss, and it buys a scheduler with no nested submission, no counter that can be released by the wrong
// thread, and no stack growth proportional to the element count.
//
// The one rule that keeps nesting safe survives from design 3: **a counter covers exactly one submitted job**.
// The inline piece is counted by nothing, because the thread running it is the thread waiting -- it knows
// when it has finished without being told.

void JobSystem::parallelFor(const uint32_t count, const uint32_t grainSize,
        const std::function<void(uint32_t, uint32_t)>& range) {
  if (count == 0 || range == nullptr) return;

  const uint32_t grain = grainSize != 0 ? grainSize : std::max(1u, count / std::max(1u, mWorkerCount));

  // One piece, or no pool to run it on: do it here. Not a fallback but the right answer -- the split costs
  // more than a single grain-sized slice of work, and a pool whose only thread is the caller has nobody to
  // hand a piece to.
  if (count <= grain || mWorkerCount <= 1) {
    range(0, count);
    return;
  }

  // Pieces of at least one grain and at most `mWorkerCount` of them, so the split is bounded by the pool
  // rather than by the element count.
  const uint32_t pieceCount = std::min(mWorkerCount, count / grain);
  const uint32_t pieceSize = count / pieceCount + (count % pieceCount != 0 ? 1u : 0u);

  // The pieces this thread does not take. The counters are collected first and waited on last, so the number
  // of live counters is exactly the number of pieces minus one -- and each is released by the single job that
  // covers it.
  std::vector<std::shared_ptr<Counter>> pending;
  pending.reserve(pieceCount > 0 ? pieceCount - 1 : 0);

  const auto rangePtr = &range;
  uint32_t begin = 0;
  for (uint32_t piece = 0; piece < pieceCount; ++piece) {
    const uint32_t end = std::min(count, begin + pieceSize);
    if (begin >= end) break;

    // Piece 0 runs here. The rest are submitted: a submitted piece is the whole of its job, so a job never
    // spawns another job and the pool cannot be starved by its own work.
    if (piece != 0) {
      auto counter = std::make_shared<Counter>();
      counter->acquire(1);
      pending.push_back(counter);
      submit([rangePtr, begin, end, counter] {
        (*rangePtr)(begin, end);
        counter->release();
      }, *counter);
    }

    begin = end;
  }

  range(0, std::min(count, pieceSize));

  // Waited on in submission order: the pieces were cut front to back, and the first submitted ones are the
  // ones least likely to have been helped by a thief yet.
  for (const std::shared_ptr<Counter>& counter: pending) {
    waitAndReset(*counter);
  }
}

void JobSystem::parallelForAsync(const uint32_t count, const uint32_t grainSize, Counter& counter,
        const std::function<void(uint32_t, uint32_t)>& range) {
  // ## Why this distributes into the pool and then waits, rather than returning immediately
  //
  // The name promises "queue this work and let me wait once at the end", and the obvious implementation is a
  // single submitted job that runs the split. It does not work, and the reason is worth recording: a job that
  // runs a split must wait for the pieces it spawns, and a pool with every worker inside such a wait has no
  // thread left to run the pieces. Two variants of that shape were written and both deadlocked.
  //
  // What this does instead is split the range into one piece per worker *here*, submit every piece but one,
  // run the last on the calling thread, and release the caller's counter when the last piece is done. No
  // submitted job spawns or waits for anything, so the starvation cannot happen; and the counter the caller
  // holds reaches zero exactly when the work does, which is the property its `wait` needs.
  //
  // The contract that changes: this call may return later than "after queueing" -- it returns after the work
  // it distributed has finished. A caller that wanted an overlap between two independent ranges should submit
  // both from the pool rather than calling this twice; `submit` is the API for that, and this stays the simple
  // "parallelise this one range" call it is named for.
  if (count == 0 || range == nullptr) {
    // Nothing is going to run, so the caller's acquisition is released here rather than left outstanding.
    counter.release();
    return;
  }

  const uint32_t grain = grainSize != 0 ? grainSize : std::max(1u, count / std::max(1u, mWorkerCount));

  if (count <= grain || mWorkerCount <= 1) {
    range(0, count);
    counter.release();
    return;
  }

  const uint32_t pieceCount = std::min(mWorkerCount, count / grain);
  const uint32_t pieceSize = count / pieceCount + (count % pieceCount != 0 ? 1u : 0u);

  // The pieces that are not this thread's. Each is released by the one job that covers it, and the caller's
  // counter is released by the job that covers the last piece -- so the caller's count reaches zero when the
  // whole range is done, not when it has merely been queued.
  const auto rangePtr = &range;

  // Submitted *before* the inline piece is run, so every worker can start immediately rather than waiting for
  // this thread to reach the submission loop.
  std::vector<std::shared_ptr<Counter>> pending;
  pending.reserve(pieceCount > 0 ? pieceCount - 1 : 0);

  uint32_t begin = pieceSize;
  for (uint32_t piece = 1; piece < pieceCount; ++piece) {
    const uint32_t end = std::min(count, begin + pieceSize);
    if (begin >= end) break;

    auto pieceCounter = std::make_shared<Counter>();
    pieceCounter->acquire(1);
    pending.push_back(pieceCounter);
    submit([rangePtr, begin, end, pieceCounter] {
      (*rangePtr)(begin, end);
      pieceCounter->release();
    }, *pieceCounter);

    begin = end;
  }

  range(0, std::min(count, pieceSize));

  for (const std::shared_ptr<Counter>& pieceCounter: pending) {
    waitAndReset(*pieceCounter);
  }

  // The caller's own acquisition, released last: this is the release its `wait` is waiting for, so it must
  // happen after every piece -- including the inline one -- has finished.
  counter.release();
}

} // namespace utils