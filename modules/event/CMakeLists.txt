cmake_minimum_required(VERSION 3.19)
set(TARGET event)
set(PUBLIC_HDR_DIR ${CORALCORE_INCLUDE_DIR})

# ==================================================================================================
# Sources and headers
# ==================================================================================================
file(GLOB_RECURSE PUBLIC_HDRS ${PUBLIC_HDR_DIR}/core/${TARGET}/*.h)

set(PUBLIC_HDRS
    ${PUBLIC_HDR_DIR}/core/event/Definitions.h
    ${PUBLIC_HDR_DIR}/core/event/EventHandler.h
    ${PUBLIC_HDR_DIR}/core/event/EventLooper.h
    ${PUBLIC_HDR_DIR}/core/event/EventThread.h
    ${PUBLIC_HDR_DIR}/core/event/Message.h
    ${PUBLIC_HDR_DIR}/core/event/MessagePool.h
    ${PUBLIC_HDR_DIR}/core/event/QueuedTask.h
    ${PUBLIC_HDR_DIR}/core/event/Timer.h
)

set(SRCS
    src/BlockingWait.h
    src/EventHandler.cpp
    src/EventLooper.cpp
    src/EventThread.cpp
    src/LockFreeMpscQueue.h
    src/Message.cpp
    src/MessageBuffer.h
    src/MessagePool.cpp
    src/MessageQueue.cpp
    src/Timer.cpp
    src/MessageQueue.h
)

# ==================================================================================================
# Include and target definitions
# ==================================================================================================
include_directories(${PUBLIC_HDR_DIR})
include_directories(src)

add_library(${TARGET} STATIC ${PRIVATE_HDRS} ${PUBLIC_HDRS} ${SRCS})

set(_coral_public_include "${PUBLIC_HDR_DIR}")
if (NOT IS_ABSOLUTE "${_coral_public_include}")
  set(_coral_public_include "${CMAKE_CURRENT_SOURCE_DIR}/${_coral_public_include}")
endif ()
target_include_directories(${TARGET} PUBLIC $<BUILD_INTERFACE:${_coral_public_include}> $<INSTALL_INTERFACE:include>)
set_target_properties(${TARGET} PROPERTIES FOLDER Libs)

# ==================================================================================================
# Dependencies
# ==================================================================================================
target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:minilog>)

if (ANDROID)
  target_link_libraries(${TARGET} PUBLIC log)
  target_link_libraries(${TARGET} PRIVATE dl)
  target_link_libraries(${TARGET} PUBLIC android)
endif ()

if (APPLE)
  # Needed for NSTemporaryDirectory()
  target_link_libraries(${TARGET} PRIVATE "-framework Foundation")
endif ()

if (LINUX)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:Threads::Threads>)
  target_link_libraries(${TARGET} PRIVATE dl)
endif ()

if (WIN32)
  # winmm is for the multimedia timer; Synchronization provides WaitOnAddress and
  # WakeByAddress*, which the lock-free wait primitive is built on.
  target_link_libraries(${TARGET} PRIVATE winmm Synchronization)
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
install(TARGETS ${TARGET} EXPORT CoralUITargets ARCHIVE DESTINATION lib)

# ==================================================================================================
# Tests
# ==================================================================================================
if (CORAL_BUILD_TESTS)

  enable_testing()
  add_subdirectory(tests)

endif ()

