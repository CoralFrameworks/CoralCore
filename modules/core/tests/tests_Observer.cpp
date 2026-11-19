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
//  tests_Observer.cpp
//
//  Covers the standalone observer module:
//
//    - Property notifies the old and the new value, and skips equal values
//    - the initial callback, the executor and cancellation
//    - an Observation outliving its Property
//    - Event delivery, cancellation and self-cancellation during dispatch
//    - a callback removing another observer during the same dispatch
//
//  Every assertion is deterministic: the tests use an inline or a manually drained executor, never
//  a timing assumption.
// =============================================================

#include <core/observer/Observer.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace coral::observer;

namespace {

/** An executor that stores tasks until the test runs them. */
class QueueExecutor final : public Executor {
public:
  void post(std::function<void()> task) override {
    tasks.push_back(std::move(task));
  }

  void runAll() {
    std::vector<std::function<void()>> pending;
    pending.swap(tasks);
    for (auto& task: pending) {
      if (task != nullptr) {
        task();
      }
    }
  }

  [[nodiscard]] std::size_t size() const { return tasks.size(); }

  std::vector<std::function<void()>> tasks;
};

} // namespace

TEST(ObserverPropertyTest, NotifiesTheOldAndTheNewValue) {
  Property<int> count(1);
  std::vector<std::pair<int, int>> changes;
  auto observation = count.observe([&changes](const int& oldValue, const int& newValue) {
    changes.emplace_back(oldValue, newValue);
  });

  count.set(2);
  count.set(2);
  count.set(3);

  ASSERT_EQ(changes.size(), 2u);
  EXPECT_EQ(changes[0], std::make_pair(1, 2));
  EXPECT_EQ(changes[1], std::make_pair(2, 3));
  EXPECT_EQ(count.get(), 3);
}

TEST(ObserverPropertyTest, CanNotifyForEqualValues) {
  Property<std::string> name("alpha", true);
  int calls = 0;
  auto observation = name.observe([&calls](const std::string&, const std::string&) { ++calls; });

  name.set("alpha");

  EXPECT_EQ(calls, 1);
}

TEST(ObserverPropertyTest, CallsTheObserverImmediatelyWhenAsked) {
  Property<int> count(7);
  int oldValue = 0;
  int newValue = 0;
  auto observation = count.observe([&oldValue, &newValue](const int& old, const int& value) {
    oldValue = old;
    newValue = value;
  }, nullptr, true);

  EXPECT_EQ(oldValue, 7);
  EXPECT_EQ(newValue, 7);
}

TEST(ObserverPropertyTest, ResetStopsTheNotifications) {
  Property<int> count(0);
  int calls = 0;
  auto observation = count.observe([&calls](const int&, const int&) { ++calls; });
  count.set(1);
  EXPECT_EQ(calls, 1);

  observation.reset();
  count.set(2);
  EXPECT_EQ(calls, 1);
  EXPECT_FALSE(observation.valid());
}

TEST(ObserverPropertyTest, AnObservationMayOutliveItsProperty) {
  Observation observation;
  {
    Property<int> count(0);
    observation = count.observe([](const int&, const int&) {});
    count.set(1);
  }
  // The Property is gone; destroying the observation must not reach it.
  observation.reset();
}

TEST(ObserverPropertyTest, DeliversThroughAnExecutor) {
  Property<int> count(0);
  auto executor = std::make_shared<QueueExecutor>();
  int calls = 0;
  int seen = 0;
  auto observation = count.observe([&calls, &seen](const int&, const int& value) {
    ++calls;
    seen = value;
  }, executor);

  count.set(5);
  EXPECT_EQ(calls, 0);
  ASSERT_EQ(executor->size(), 1u);
  executor->runAll();
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(seen, 5);

  observation.reset();
  count.set(6);
  EXPECT_EQ(executor->size(), 0u);
}

TEST(ObserverPropertyTest, AQueuedCallbackIsSkippedAfterReset) {
  Property<int> count(0);
  auto executor = std::make_shared<QueueExecutor>();
  int calls = 0;
  auto observation = count.observe([&calls](const int&, const int&) { ++calls; }, executor);
  count.set(1);
  ASSERT_EQ(executor->size(), 1u);

  observation.reset();
  executor->runAll();
  EXPECT_EQ(calls, 0);
}

TEST(ObserverPropertyTest, AllowsReentrantWrites) {
  Property<int> count(0);
  int calls = 0;
  auto observation = count.observe([&count, &calls](const int&, const int& value) {
    ++calls;
    if (value == 1) {
      count.set(2);
    }
  });

  count.set(1);

  EXPECT_EQ(calls, 2);
  EXPECT_EQ(count.get(), 2);
}

TEST(ObserverPropertyTest, NotifiesATypeWithoutEquality) {
  struct NoEquality {
    int value = 0;
  };

  Property<NoEquality> property(NoEquality{ 1 });
  int calls = 0;
  auto observation = property.observe([&calls](const NoEquality&, const NoEquality&) { ++calls; });

  property.set(NoEquality{ 1 });

  EXPECT_EQ(calls, 1);
}

TEST(ObserverEventTest, DeliversTypedPayloads) {
  struct Changed {
    int value = 0;
  };

  Event<Changed> changed;
  std::vector<int> values;
  auto observation = changed.observe([&values](const Changed& event) {
    values.push_back(event.value);
  });

  changed.emit(Changed{ 1 });
  changed.emit(Changed{ 2 });

  ASSERT_EQ(values.size(), 2u);
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(values[1], 2);
}

TEST(ObserverEventTest, ResetStopsTheEvent) {
  struct Tick {
  };

  Event<Tick> tick;
  int calls = 0;
  auto observation = tick.observe([&calls](const Tick&) { ++calls; });
  tick.emit(Tick{});
  observation.reset();
  tick.emit(Tick{});
  EXPECT_EQ(calls, 1);
}

TEST(ObserverEventTest, AnObserverCanCancelItselfDuringDispatch) {
  struct Tick {
  };

  Event<Tick> tick;
  int calls = 0;
  Observation observation;
  observation = tick.observe([&calls, &observation](const Tick&) {
    ++calls;
    observation.reset();
  });

  tick.emit(Tick{});
  tick.emit(Tick{});
  EXPECT_EQ(calls, 1);
}

TEST(ObserverEventTest, RemovingAnotherObserverDuringDispatchSkipsIt) {
  struct Tick {
  };

  Event<Tick> tick;
  int first = 0;
  int second = 0;
  Observation secondObservation;
  auto firstObservation = tick.observe([&first, &secondObservation](const Tick&) {
    ++first;
    secondObservation.reset();
  });
  secondObservation = tick.observe([&second](const Tick&) { ++second; });

  tick.emit(Tick{});

  EXPECT_EQ(first, 1);
  EXPECT_EQ(second, 0);
}
