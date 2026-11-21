cmake_minimum_required(VERSION 3.19)
set(TARGET core)

set(PUBLIC_HDR_DIR ${CORALCORE_INCLUDE_DIR})

# ==================================================================================================
# Sources and headers
# ==================================================================================================
file(GLOB_RECURSE PUBLIC_HDRS ${PUBLIC_HDR_DIR}/${TARGET}/*.h)

set(PUBLIC_HDRS
    ${PUBLIC_HDR_DIR}/core/task/AtomicWait.h
    ${PUBLIC_HDR_DIR}/core/task/ConditionVariable.h
    ${PUBLIC_HDR_DIR}/core/task/CountDownLatch.h
    ${PUBLIC_HDR_DIR}/core/task/LockFreeMpscQueue.h
    ${PUBLIC_HDR_DIR}/core/task/Mutex.h
    ${PUBLIC_HDR_DIR}/core/task/Semaphore.h
    ${PUBLIC_HDR_DIR}/core/task/SpinLock.h
    ${PUBLIC_HDR_DIR}/core/task/TaskBase.h
    ${PUBLIC_HDR_DIR}/core/task/ThreadSafety.h
    ${PUBLIC_HDR_DIR}/core/task/ThreadUtils.h
    ${PUBLIC_HDR_DIR}/core/time/SystemClock.h
    ${PUBLIC_HDR_DIR}/core/time/Stopwatch.h
)

set(SRCS
    src/task/CountDownLatch.cpp
    src/task/eintr_wrapper.h
    src/task/Semaphore.cpp
    src/task/SpinLock.cpp
    src/task/ThreadUtils.cpp
    src/time/SystemClock.cpp
    src/time/Stopwatch.cpp
)

# ==================================================================================================
# Platform Sources
# ==================================================================================================
if (ANDROID OR LINUX)
  list(APPEND SRCS
      src/platform/linux/PosixStopwatch.h
      src/platform/linux/PosixStopwatch.cpp
  )
endif ()

if (APPLE)
  list(APPEND SRCS
      src/platform/darwin/DarwinStopwatch.cpp
      src/platform/darwin/DarwinStopwatch.h
  )
endif ()

if (WIN32)
  list(APPEND SRCS
      src/platform/win32/WindowsStopwatch.cpp
      src/platform/win32/WindowsStopwatch.h
  )
endif ()

# ==================================================================================================
# Include and target definitions
# ==================================================================================================
include_directories(${PUBLIC_HDR_DIR})
include_directories(src)

add_library(${TARGET} STATIC ${PRIVATE_HDRS} ${PUBLIC_HDRS} ${SRCS})

target_include_directories(${TARGET} PUBLIC ${PUBLIC_HDR_DIR})

set_target_properties(${TARGET} PROPERTIES FOLDER Libs)

# ==================================================================================================
# Dependencies
# ==================================================================================================
target_link_libraries(${TARGET} PRIVATE minilog)
target_link_libraries(${TARGET} PUBLIC utils)

if (ANDROID)
  target_link_libraries(${TARGET} PUBLIC log)
  target_link_libraries(${TARGET} PRIVATE dl)
  target_link_libraries(${TARGET} PUBLIC android)
endif ()

if (LINUX)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(${TARGET} PRIVATE Threads::Threads)
  target_link_libraries(${TARGET} PRIVATE dl)
endif ()

if (WIN32)
  # Synchronization provides WaitOnAddress and WakeByAddress*, the lock-free wait primitive used by
  # AtomicWait/CountDownLatch. The old multimedia timer went away with the platform MessageLoop.
  target_link_libraries(${TARGET} PRIVATE Synchronization)
endif ()

# ==================================================================================================
# Compiler flags
# ==================================================================================================
if (MSVC)
  target_compile_options(${TARGET} PRIVATE $<$<CONFIG:Release>:/fp:fast>)
else ()
  target_compile_options(${TARGET} PRIVATE $<$<CONFIG:Release>:-ffast-math -fno-finite-math-only>)
  target_compile_options(${TARGET} PRIVATE -Wno-deprecated-register)
endif ()

target_compile_features(${TARGET} PRIVATE cxx_strong_enums)

# ==================================================================================================
# Installation
# ==================================================================================================
install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

# Public headers live in the framework's root include/core/{task,time}/ and are included as
# `<core/task/...>` / `<core/time/...>`. The root CMakeLists installs the whole include/ tree.

# ==================================================================================================
# observer (header-only)
# ==================================================================================================
# observer has no source file and no dependencies. It lives in core/include/core/observer and is
# exported as its own INTERFACE target so consumers that only need observation do not link the
# event loop or Minilog.
add_library(observer INTERFACE)
target_include_directories(observer INTERFACE
    $<BUILD_INTERFACE:${PUBLIC_HDR_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(observer INTERFACE cxx_std_20)
set_target_properties(observer PROPERTIES FOLDER Libs)

install(TARGETS observer EXPORT CoralCoreTargets)
install(EXPORT CoralCoreTargets
    NAMESPACE Coral::
    DESTINATION lib/cmake/CoralCore
)

if (CORAL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif ()
