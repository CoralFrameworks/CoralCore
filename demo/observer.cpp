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
//  observer.cpp
//
//  `observer` is header-only: Property for a value that changes,
//  Event for a discrete notification, and Observation as the RAII
//  handle that unregisters. The demo links only `observer`, which
//  proves a consumer of the model layer does not inherit `core`,
//  `event` or Minilog.
//
//    coralcore_demo_observer
// =============================================================

#include <core/observer/Event.h>
#include <core/observer/Executor.h>
#include <core/observer/Observation.h>
#include <core/observer/Property.h>

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
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

struct Note {
  std::string name;
  int velocity = 0;
};

} // namespace

int main() {
  std::printf("CoralCore demo: observer\n\n");

  // ------------------------------------------------------------------------------------------------
  // Property: the callback receives the value before and after the change.
  // ------------------------------------------------------------------------------------------------
  coral::observer::Property<int> volume(50);
  check(volume.get() == 50, "a Property holds its initial value");

  std::vector<std::pair<int, int>> changes;
  coral::observer::Observation subscription = volume.observe(
          [&changes](const int& oldValue, const int& newValue) { changes.emplace_back(oldValue, newValue); });
  check(subscription.valid(), "observe() returns a valid Observation");

  volume.set(75);
  volume.set(75); // assigning the current value does not notify
  volume.set(100);

  check(changes.size() == 2, "only real changes notify");
  check(changes.size() == 2 && changes[0] == std::make_pair(50, 75), "the callback receives the old and new value");
  std::printf("  volume changes: %zu\n", changes.size());

  // Destroying, resetting or assigning over an Observation unregisters it.
  subscription.reset();
  check(!subscription.valid(), "reset() invalidates the Observation");
  volume.set(0);
  check(changes.size() == 2, "a reset Observation is not called again");

  // ------------------------------------------------------------------------------------------------
  // An Executor decides where the callback runs. InlineExecutor runs it on the calling thread, which
  // is what a test or a model without a UI needs; a GUI would pass an executor over its main loop.
  // ------------------------------------------------------------------------------------------------
  const std::shared_ptr<coral::observer::InlineExecutor> executor = std::make_shared<coral::observer::InlineExecutor>();

  int initialCalls = 0;
  coral::observer::Observation initialSubscription =
          volume.observe([&initialCalls](const int&, const int&) { ++initialCalls; }, executor, true);
  check(initialCalls == 1, "observe(initial=true) calls the observer once immediately");

  volume.set(25);
  check(initialCalls == 2, "the executor delivers the later change too");

  // ------------------------------------------------------------------------------------------------
  // Event: a discrete notification with a typed payload. Every observer registered at emit time is
  // called, and an Observation that was reset before the emit is not.
  // ------------------------------------------------------------------------------------------------
  coral::observer::Event<Note> noteOn;
  std::vector<std::string> heard;
  coral::observer::Observation listener = noteOn.observe([&heard](const Note& note) { heard.push_back(note.name); });

  noteOn.emit(Note{ "C4", 64 });
  noteOn.emit(Note{ "E4", 60 });
  check(heard.size() == 2 && heard[1] == "E4", "an Event delivers every emit to its observers");

  listener.reset();
  noteOn.emit(Note{ "G4", 67 });
  check(heard.size() == 2, "a reset Event listener is not called");

  // ------------------------------------------------------------------------------------------------
  // An Observation is the handle, so moving it transfers the registration without unregistering.
  // ------------------------------------------------------------------------------------------------
  int movedCalls = 0;
  coral::observer::Observation original = noteOn.observe([&movedCalls](const Note&) { ++movedCalls; });
  coral::observer::Observation moved = std::move(original);

  check(!original.valid() && moved.valid(), "moving an Observation transfers the registration");
  noteOn.emit(Note{ "A4", 69 });
  check(movedCalls == 1, "the moved Observation still receives events");

  std::printf("\n%d checks, %d failures\n", gChecks, gFailures);
  std::printf("%s\n", gFailures == 0 ? "coralcore_demo_observer: PASS" : "coralcore_demo_observer: FAIL");
  return gFailures == 0 ? 0 : 1;
}
