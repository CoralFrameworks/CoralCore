# CoralCore demos

Five small programs that show the intended use of each module. They are also the standalone
verification: every one of them checks what it observed and returns a non-zero exit code when a check
fails, so building and running them proves that a checkout compiles, links and executes.

They are built when `CORALCORE_BUILD_DEMOS` is on, which is the default when CoralCore is the
top-level CMake project. When CoralCore is added to an application tree the demos stay off; enable
them there with `-DCORALCORE_BUILD_DEMOS=ON`.

## Build and run

From the root of the repository, after initializing the submodules:

```sh
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/demo/coralcore_demo_hello             # Windows: build\demo\coralcore_demo_hello.exe
./build/demo/coralcore_demo_utils
./build/demo/coralcore_demo_observer
./build/demo/coralcore_demo_event_loop
./build/demo/coralcore_demo_job_system
```

The demos are registered with CTest when testing is enabled, which is the default for a standalone
checkout, so one command runs them together with the test suites:

```sh
ctest --test-dir build --output-on-failure
```

The libraries and the demos do not need GoogleTest. If the submodules are not initialized, configure
with the test suites off:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCORAL_BUILD_TESTS=OFF
cmake --build build
```

With a multi-configuration generator (Visual Studio, Xcode) the executables are in
`build/demo/<Configuration>/` and the build command needs `--config <Configuration>`.

## What each demo shows

| Executable | Module | Shows |
| --- | --- | --- |
| `coralcore_demo_hello` | `core` | `SystemClock`, a platform `Stopwatch`, and a `CountDownLatch` counted down by four threads. |
| `coralcore_demo_utils` | `utils` | A `Base64` round trip, `Utf` code-point counting and conversion, and `Path` string operations. |
| `coralcore_demo_observer` | `observer` | `Property` change notification, `Event` delivery, `Observation` lifetime and an `InlineExecutor`. |
| `coralcore_demo_event_loop` | `event` | An `EventThread` with an `EventHandler`: a posted closure, a `Message`, a delayed closure and a repeating `event::Timer`. |
| `coralcore_demo_job_system` | `utils` | `JobSystem::parallelFor()` over 200 000 items, plus a `submit()`/`Counter` batch. |

Each demo links only the module it demonstrates, so the include paths and libraries it needs are the
same ones a consumer of that module needs.

## Exit codes

Every demo prints `ok`/`FAIL` per check and a final `PASS`/`FAIL` line. The exit code is `0` when every
check passed and `1` otherwise, so a script can run the binaries without parsing their output.

## Notes

* No demo writes to disk or reads a file; `coralcore_demo_utils` reads the current working directory
  through `Path::getCurrentDirectory()` only.
* The waits in `coralcore_demo_event_loop` are bounded (three seconds), so a regression fails the
  demo instead of hanging it.
* The demos are examples, not tests: the exhaustive suites live under `modules/*/tests` and use
  GoogleTest.
