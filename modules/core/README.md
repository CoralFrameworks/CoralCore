# libs/core

Foundation layer shared by the media engine and the UI framework. It is deliberately a directory of
small targets rather than one monolithic library.

## Targets

| Target | Contents | Dependencies |
|---|---|---|
| `core` | synchronization primitives, thread utilities, monotonic `Stopwatch`, `SystemClock` | `utils`, `minilog` |
| `event` | `EventLooper`, `EventThread`, `EventHandler`, `MessageQueue`, `event::Timer` | `minilog` |
| `observer` | header-only `Property`, `Event`, `Observation`, `Observer` | none |

`ui` links `observer` (and `graphics`); the media engine links `core`. Keeping them separate means a
consumer of `observer` does not inherit `event`'s threading/platform dependencies, and the media
engine does not link the UI's event loop.

`libs/mvvm` is the view-model layer above `observer` (`ObservableObject`, `Command`, `AsyncCommand`,
`ObservableList`, `Validation`, `Field`). It is a separate target and does not depend on `ui`, so it
can be tested without a window or a device.

## Header paths

```cpp
#include <core/task/Mutex.h>
#include <core/task/CountDownLatch.h>
#include <core/time/Stopwatch.h>
#include <event/EventLooper.h>
#include <event/Timer.h>
#include <observer/Property.h>
```

The `core/` prefix is intentional: these headers are general foundation code, not an API owned by the
Coral application. They are installed under `include/core/...`, `include/event/...` and
`include/observer/...`.

## Boundaries

- `core/task` keeps only synchronization primitives and thread utilities. The old platform
  `MessageLoop`/`TaskRunner`/`Executor` implementation was removed when `event` became the
  only loop.
- `event` is the only task/message loop. It is a thread-local, lock-free queue with sync barriers,
  async messages and cancellation; it does not own a window or a platform event source.
- `core::Stopwatch` is a monotonic stopwatch (`start`/`stop`/`getElapsedTime`/`getAbsoluteTime`). It
  does not schedule callbacks; use `event::Timer` for delayed/repeating work on a looper.
- `gpu::FrameClock` remains the frame-deadline/vsync scheduler; do not drive frames from a
  `Stopwatch`.

## Tests

- `tests_EventLoop` (under `libs/core/event/tests`) covers the event queue and `event::Timer`.
- The Android `ALooper` fd bridge is not implemented yet; it belongs on `EventLooper` as an optional
  platform source rather than a second MessageLoop.
