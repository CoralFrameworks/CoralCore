# ==================================================================================================
# CoralCore
# ==================================================================================================
# The foundation framework: utils, core, event and observer. Minilog is a separate repository and is
# carried as a nested submodule under modules/minilog for a standalone checkout; the tests use
# GoogleTest from the third_party/libgtest submodule. Initialize both before configuring:
#
#   git submodule update --init --recursive
#
# A standalone checkout builds the tests and the demos:
#
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   ctest --test-dir build --output-on-failure
#
# The libraries and the demos do not need GoogleTest, so a checkout without the submodules can still
# be verified with the tests off:
#
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCORAL_BUILD_TESTS=OFF
#   cmake --build build
#   ./build/demo/coralcore_demo_hello

cmake_minimum_required(VERSION 3.19)
project(CoralCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/CoralBuild.cmake)
coral_apply_global_build_settings()

# One public include root for the whole framework: core, event, observer, utils and jni all live
# under include/.
set(CORALCORE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# The Minilog CMakeLists uses the same top-level pattern: a standalone checkout gets its own defaults,
# while a parent project's settings win.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CORALCORE_TOP_LEVEL ON)
else ()
  set(CORALCORE_TOP_LEVEL OFF)
endif ()

# Tests default to on for a standalone checkout, and off when CoralCore is added to an application
# tree, where the application's CORAL_BUILD_TESTS decides. CORAL_BUILD_TESTS is the public switch: a
# caller that passes -DCORAL_BUILD_TESTS=OFF gets no tests, including when CoralCore is top-level.
if (CORALCORE_TOP_LEVEL)
  if (NOT DEFINED IS_HOST_PLATFORM)
    set(IS_HOST_PLATFORM TRUE)
  endif ()
  set(_coralcore_tests_default ON)
else ()
  set(_coralcore_tests_default OFF)
endif ()
option(CORAL_BUILD_TESTS "Build the CoralCore tests" ${_coralcore_tests_default})
set(CORALCORE_BUILD_TESTS ${CORAL_BUILD_TESTS} CACHE BOOL "Build the CoralCore tests" FORCE)

if (CORAL_BUILD_TESTS)
  enable_testing()
endif ()

# The demos in demo/ are usage examples and a standalone smoke test: each one exercises a module and
# checks its result. They are built for a standalone checkout only; an application tree does not need
# example executables, but can ask for them with -DCORALCORE_BUILD_DEMOS=ON.
option(CORALCORE_BUILD_DEMOS "Build the CoralCore demos in demo/" ${CORALCORE_TOP_LEVEL})

# Minilog: prefer an existing target (the application root), then the nested submodule, then an
# explicit directory or the sibling checkout. The nested submodule is preferred for a standalone
# CoralCore checkout; the sibling form keeps the monorepo layout working.
if (NOT TARGET minilog)
  if (CORALCORE_TOP_LEVEL AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/minilog/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/modules/minilog" "${CMAKE_BINARY_DIR}/minilog")
  elseif (DEFINED CORAL_MINILOG_DIR AND EXISTS "${CORAL_MINILOG_DIR}/CMakeLists.txt")
    add_subdirectory("${CORAL_MINILOG_DIR}" "${CMAKE_BINARY_DIR}/minilog")
  elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/minilog/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/modules/minilog" "${CMAKE_BINARY_DIR}/minilog")
  else ()
    message(FATAL_ERROR
        "CoralCore needs Minilog. Add it as modules/minilog (submodule), a sibling Minilog, "
        "pass -DCORAL_MINILOG_DIR=<path>, or add the minilog target before CoralCore.")
  endif ()
endif ()

# Tests use GoogleTest. A standalone CoralCore gets it from its third_party/libgtest submodule
# (which carries its own googletest submodule); CORAL_GTEST_DIR overrides that. As a subproject,
# CoralCore deliberately does not add its own copy: the application root's third_party/libgtest is
# the one gtest target in the build.
if (CORAL_BUILD_TESTS)
  if (CORALCORE_TOP_LEVEL AND NOT TARGET gtest)
    if (DEFINED CORAL_GTEST_DIR AND EXISTS "${CORAL_GTEST_DIR}/CMakeLists.txt")
      add_subdirectory("${CORAL_GTEST_DIR}" "${CMAKE_BINARY_DIR}/gtest")
    elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libgtest/coral/CMakeLists.txt")
      add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/libgtest/coral" "${CMAKE_BINARY_DIR}/gtest")
    else ()
      message(FATAL_ERROR
          "CoralCore tests need GoogleTest. Initialize the submodules with "
          "'git submodule update --init --recursive', pass -DCORAL_GTEST_DIR=<path>, or configure "
          "with -DCORAL_BUILD_TESTS=OFF.")
    endif ()
  elseif (NOT CORALCORE_TOP_LEVEL AND NOT TARGET gtest)
    message(FATAL_ERROR
        "CoralCore is a subproject: the parent project must provide the gtest target from its "
        "third_party/libgtest before adding CoralCore.")
  endif ()
endif ()

add_subdirectory(modules/utils)
add_subdirectory(modules/core)
add_subdirectory(modules/event)

# JNI is host/Android-only: Android gets jni.h from the NDK; a host build needs a JDK.
if (ANDROID)
  add_subdirectory(modules/jni)
elseif (IS_HOST_PLATFORM)
  find_package(JNI QUIET)
  if (JNI_FOUND)
    add_subdirectory(modules/jni)
  else ()
    message(STATUS "CoralCore: JNI support disabled: jni.h was not found")
  endif ()
endif ()

if (CORALCORE_BUILD_DEMOS)
  add_subdirectory(demo)
endif ()
