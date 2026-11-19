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

#ifndef CORAL_OBSERVER_OBSERVATION_H
#define CORAL_OBSERVER_OBSERVATION_H

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

namespace coral::observer::detail {

/**
 * The shared state of one registration.
 *
 * The owner of a Property or Event keeps the callback in its observer list; this entry is what the
 * Observation returned to the caller holds. Cancelling it flips `active` first, so a callback that
 * was already copied into a dispatch snapshot is skipped, and then removes the registration from
 * the owner.
 */
struct Entry {
  std::atomic_bool active { true };
  std::function<void()> cancel;
};

} // namespace coral::observer::detail

namespace coral::observer {

/**
 * A registered observation.
 *
 * Observation is move-only and owns the registration: destroying it, assigning over it or calling
 * reset() unregisters the observer. A registration that outlives its Property or Event is harmless:
 * the owner is held weakly by the cancellation and is simply gone.
 */
class Observation {
public:
  Observation() noexcept = default;

  explicit Observation(std::shared_ptr<detail::Entry> entry) noexcept
    : mEntry(std::move(entry)) {
  }

  ~Observation() {
    reset();
  }

  Observation(const Observation&) = delete;
  Observation& operator=(const Observation&) = delete;

  Observation(Observation&& other) noexcept
    : mEntry(std::move(other.mEntry)) {
  }

  Observation& operator=(Observation&& other) noexcept {
    if (this != &other) {
      reset();
      mEntry = std::move(other.mEntry);
    }
    return *this;
  }

  /** Unregisters the observer. Safe to call more than once. */
  void reset() noexcept {
    if (mEntry == nullptr) {
      return;
    }
    mEntry->active.store(false, std::memory_order_release);
    if (mEntry->cancel != nullptr) {
      mEntry->cancel();
    }
    mEntry.reset();
  }

  [[nodiscard]] bool valid() const noexcept {
    return mEntry != nullptr && mEntry->active.load(std::memory_order_acquire);
  }

private:
  std::shared_ptr<detail::Entry> mEntry;
};

} // namespace coral::observer

#endif // CORAL_OBSERVER_OBSERVATION_H
