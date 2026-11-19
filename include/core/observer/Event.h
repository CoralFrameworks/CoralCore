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

#ifndef CORAL_OBSERVER_EVENT_H
#define CORAL_OBSERVER_EVENT_H

#include "core/observer/Executor.h"
#include "core/observer/Observation.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace coral::observer {

/**
 * A typed event that a model emits and a consumer observes.
 *
 * Event is the discrete counterpart of Property: the payload is a plain C++ struct, so there is no
 * string key and no type-erased user info. A model owns an Event, exposes it as
 * `const Event<E>&`, and emits it; a consumer registers an observer and receives every event on the
 * thread that emitted it unless an Executor was given.
 *
 * The callback is copied out of the registration list before it runs, so an observer may cancel
 * itself from the callback and a callback that was already in the dispatch snapshot is skipped once
 * its Observation is reset. E must be copy-constructible because an Executor carries a copy of the
 * event to the thread that runs the callback.
 */
template <class E>
class Event {
public:
  Event() = default;
  ~Event() = default;

  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;
  Event(Event&&) = delete;
  Event& operator=(Event&&) = delete;

  /** Delivers @p event to every observer registered at the time of the call. */
  void emit(const E& event) {
    std::vector<Registration> snapshot;
    {
      const std::lock_guard<std::mutex> lock(mState->mutex);
      snapshot = mState->observers;
    }
    // No lock is held while a callback runs: a callback may emit, observe or destroy another model.
    for (const auto& registration: snapshot) {
      dispatch(registration, event);
    }
  }

  /**
   * Registers an observer.
   *
   * @param observer Receives each emitted event.
   * @param executor Where the callback runs; null means the thread that called emit().
   */
  [[nodiscard]] Observation observe(std::function<void(const E&)> observer,
      std::shared_ptr<Executor> executor = nullptr) const {
    auto entry = std::make_shared<detail::Entry>();
    std::uint64_t id = 0;
    {
      const std::lock_guard<std::mutex> lock(mState->mutex);
      id = mState->nextId++;
      mState->observers.push_back(Registration{ id, observer, executor, entry });
    }
    entry->cancel = [state = std::weak_ptr<State>(mState), id] {
      if (const auto locked = state.lock(); locked != nullptr) {
        const std::lock_guard<std::mutex> lock(locked->mutex);
        auto& observers = locked->observers;
        for (auto it = observers.begin(); it != observers.end(); ++it) {
          if (it->id == id) {
            observers.erase(it);
            break;
          }
        }
      }
    };
    return Observation(std::move(entry));
  }

private:
  struct Registration {
    std::uint64_t id = 0;
    std::function<void(const E&)> observer;
    std::shared_ptr<Executor> executor;
    std::shared_ptr<detail::Entry> entry;
  };

  struct State {
    mutable std::mutex mutex;
    std::uint64_t nextId = 0;
    std::vector<Registration> observers;
  };

  static void dispatch(const Registration& registration, const E& event) {
    if (registration.entry == nullptr || !registration.entry->active.load(std::memory_order_acquire) ||
        registration.observer == nullptr) {
      return;
    }
    if (registration.executor == nullptr) {
      registration.observer(event);
      return;
    }
    registration.executor->post([entry = registration.entry, observer = registration.observer, event] {
      if (entry->active.load(std::memory_order_acquire)) {
        observer(event);
      }
    });
  }

  std::shared_ptr<State> mState = std::make_shared<State>();
};

} // namespace coral::observer

#endif // CORAL_OBSERVER_EVENT_H
