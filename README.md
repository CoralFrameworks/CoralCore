# CoralCore

The core foundation of the Coral frameworks: synchronization, threading, an event loop and the
observation layer. Minilog and libgtest are carried as nested submodules (`modules/minilog` and
`third_party/libgtest`).

| Target | Contents |
| --- | --- |
| `utils` | `Base64`, `Utf`, `Path`, `JobSystem` and the small value helpers |
| `core` | synchronization primitives, thread utilities, `Stopwatch`, `SystemClock` |
| `event` | `EventThread`, `EventLooper`, `EventHandler`, `Message`, `event::Timer` |
| `observer` | header-only `Property`, `Event`, `Observation` |

## Standalone build

CoralCore is a standalone CMake project. Minilog and GoogleTest (libgtest, which carries its own
googletest submodule) are nested submodules. The recursive update initializes Minilog;
`third_party/libgtest` is declared with `update = none` so that a parent project's recursive update
never downloads a copy CoralCore would not use, so a standalone checkout asks for it explicitly:

```sh
git submodule update --init --recursive
git -c submodule.third_party/libgtest.update=checkout submodule update --init third_party/libgtest
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The libraries and the demos do not depend on GoogleTest, so a checkout without the submodules can
still be built with the tests off:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCORAL_BUILD_TESTS=OFF
cmake --build build
./build/demo/coralcore_demo_hello
```

To use a GoogleTest checkout from somewhere else instead of the submodule, pass
`-DCORAL_GTEST_DIR=<path/to/gtest>`.

## Using it as a submodule

Add the checkout and link the targets you need. In an application tree the application supplies the
`minilog` and `gtest` targets; otherwise CoralCore falls back to its nested `modules/minilog` and
`third_party/libgtest` submodules. The parent's recursive update fetches Minilog; `libgtest` carries
`update = none`, so a tree that actually wants it overrides that strategy with
`git -c submodule.third_party/libgtest.update=checkout submodule update --init third_party/libgtest`.
The demos and tests default to off in that tree:

```cmake
# Keep the application and the framework on the same C++ standard and runtime library settings.
include(third_party/CoralCore/cmake/CoralBuild.cmake)
coral_apply_global_build_settings()

add_subdirectory(third_party/CoralCore)
target_link_libraries(my_app PRIVATE core event observer utils)
```

`coral_apply_global_build_settings()` is what keeps the application's targets and CoralCore's static
libraries on the same runtime library; without it a Debug build mixes `/MDd` and `/MTd` and fails to
link with `LNK2038`. `CORAL_BUILD_TESTS` and `CORALCORE_BUILD_DEMOS` can be set before
`add_subdirectory()` to build the tests and the examples in that tree as well.

## Demos

See [demo/README.md](demo/README.md): five self-checking programs, one per module, that verify the
standalone build and double as usage examples.
