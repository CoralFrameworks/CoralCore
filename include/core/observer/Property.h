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

#ifndef CORAL_OBSERVER_PROPERTY_H
#define CORAL_OBSERVER_PROPERTY_H

#include "core/observer/Executor.h"
#include "core/observer/Observation.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace coral::observer {

/**
 * A value whose changes can be observed, which is the typed C++ counterpart of a KVO property.
 *
 * Property is the model side of the observation: a model owns a Property, writes it through set(),
 * and exposes it as `const Property<T>&` plus an `observe` convenience. A consumer reads get() and
 * registers a callback with observe().
 *
 * The callback receives the value before and after the change. A set that assigns the value the
 * property already holds does not notify unless the property was created with `notifyIfEqual`; a T
 * that is not equality comparable always notifies. Observations are delivered on the thread that
 * called set() unless an Executor was given, in which case the callback is posted to that executor.
 * The current value is readable from any thread.
 *
 * A Property is not copyable or movable: its address is the identity a GUI binding may keep, and a
 * model owns it for its whole lifetime. An Observation that outlives the Property is harmless.
 *
 * T must be copy-constructible: get() returns a copy and a notification carries the old and the new
 * value to every observer.
 */
template <class T>
class Property {
public:
  explicit Property(T initialValue, const bool notifyIfEqual = false)
    : mState(std::make_shared<State>(std::move(initialValue), notifyIfEqual)) {
  }

  ~Property() = default;

  Property(const Property&) = delete;
  Property& operator=(const Property&) = delete;
  Property(Property&&) = delete;
  Property& operator=(Property&&) = delete;

  /** Returns a copy of the current value. */
  [[nodiscard]] T get() const {
    const std::lock_guard<std::mutex> lock(mState->mutex);
    return mState->value;
  }

  /** Replaces the value and notifies the observers when the value changed. */
  void set(T value) {
    std::vector<Registration> snapshot;
    std::optional<T> oldValue;
    std::optional<T> newValue;
    {
      const std::lock_guard<std::mutex> lock(mState->mutex);
      if constexpr (requires(const T& current, const T& candidate) { current == candidate; }) {
        if (!mState->notifyIfEqual && mState->value == value) {
          return;
        }
      }
      oldValue = mState->value;
      newValue = value;
      mState->value = std::move(value);
      snapshot = mState->observers;
    }
    // No lock is held here: a callback may read or write this property, or another model property.
    for (const auto& registration: snapshot) {
      dispatch(registration, *oldValue, *newValue);
    }
  }

  /**
   * Registers an observer.
   *
   * @param observer Receives the old and the new value.
   * @param executor Where the callback runs; null means the thread that called set().
   * @param initial When true the observer is also called immediately with the current value as both
   *                the old and the new value.
   */
  [[nodiscard]] Observation observe(std::function<void(const T&, const T&)> observer,
      std::shared_ptr<Executor> executor = nullptr, const bool initial = false) const {
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
    if (initial) {
      const auto value = get();
      dispatch(RegistrationView{ entry, std::move(observer), std::move(executor) }, value, value);
    }
    return Observation(std::move(entry));
  }

private:
  struct Registration {
    std::uint64_t id = 0;
    std::function<void(const T&, const T&)> observer;
    std::shared_ptr<Executor> executor;
    std::shared_ptr<detail::Entry> entry;
  };

  struct RegistrationView {
    std::shared_ptr<detail::Entry> entry;
    std::function<void(const T&, const T&)> observer;
    std::shared_ptr<Executor> executor;
  };

  struct State {
    State(T initialValue, const bool notifyEqual)
      : value(std::move(initialValue)), notifyIfEqual(notifyEqual) {
    }

    mutable std::mutex mutex;
    T value;
    bool notifyIfEqual = false;
    std::uint64_t nextId = 0;
    std::vector<Registration> observers;
  };

  static void dispatch(const Registration& registration, const T& oldValue, const T& newValue) {
    dispatch(RegistrationView{ registration.entry, registration.observer, registration.executor },
      oldValue, newValue);
  }

  static void dispatch(const RegistrationView& registration, const T& oldValue, const T& newValue) {
    if (registration.entry == nullptr || !registration.entry->active.load(std::memory_order_acquire) ||
        registration.observer == nullptr) {
      return;
    }
    if (registration.executor == nullptr) {
      registration.observer(oldValue, newValue);
      return;
    }
    registration.executor->post([entry = registration.entry, observer = registration.observer,
                                  oldValue, newValue] {
      if (entry->active.load(std::memory_order_acquire)) {
        observer(oldValue, newValue);
      }
    });
  }

  std::shared_ptr<State> mState;
};

} // namespace coral::observer

#endif // CORAL_OBSERVER_PROPERTY_H
