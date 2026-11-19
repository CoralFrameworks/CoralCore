cmake_minimum_required(VERSION 3.19)
project(utils)

set(TARGET utils)
set(PUBLIC_HDR_DIR ${CORALCORE_INCLUDE_DIR})
# Private headers (private/utils/...) are intentionally not part of the public include root.
set(PRIVATE_HDR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ==================================================================================================
# Sources and headers
# ==================================================================================================
file(GLOB_RECURSE PUBLIC_HDRS ${PUBLIC_HDR_DIR}/${TARGET}/*.h)

# The public headers. An explicit list, unlike every other module's `${PUBLIC_HDRS}` glob, and for a reason that is not
# obvious: `install(FILES)` hands each path to `file(INSTALL TYPE FILE)`, which keeps only the *file name*. The glob
# matches `utils/android/Systrace.h` and `utils/darwin/Systrace.h` as well, and installing those two would land them both
# at `include/utils/Systrace.h` -- overwriting the real one, in an order that depends on the glob's sort. Nothing reports
# it; the staged tree simply contains the wrong file.
#
# So the platform subdirectories have to stay out, and the list is how. It was two entries short:
#
#   utils/unwindows.h   included by geometry/compiler.h, which is a public header of another module
#   utils/Path.h        a public class, included by coral/audio's sources
#
# The first made the installed `geometry` headers impossible to compile against --
#
#   build/stage/include/geometry/compiler.h(20): fatal error C1083:
#     Cannot open include file: 'utils/unwindows.h'
#
# -- found by writing a program that includes `<geometry/Matrix.h>` from the staged prefix and building it. That is the
# only thing that would have noticed: the library builds either way, and the install reports success either way.
set(DIST_HDRS
    ${PUBLIC_HDR_DIR}/utils/algorithm.h
    ${PUBLIC_HDR_DIR}/utils/api_level.h
    ${PUBLIC_HDR_DIR}/utils/ashmem.h
    ${PUBLIC_HDR_DIR}/utils/Assert.h
    ${PUBLIC_HDR_DIR}/utils/Base64.h
    ${PUBLIC_HDR_DIR}/utils/bitset.h
    ${PUBLIC_HDR_DIR}/utils/ByteBuffer.h
    ${PUBLIC_HDR_DIR}/utils/CallStack.h
    ${PUBLIC_HDR_DIR}/utils/compiler.h
    ${PUBLIC_HDR_DIR}/utils/JobSystem.h
    ${PUBLIC_HDR_DIR}/utils/Panic.h
    ${PUBLIC_HDR_DIR}/utils/Path.h
    ${PUBLIC_HDR_DIR}/utils/Systrace.h
    ${PUBLIC_HDR_DIR}/utils/Utf.h
)

set(SRCS
    src/api_level.cpp
    src/ashmem.cpp
    src/Assert.cpp
    src/Base64.cpp
    src/CallStack.cpp
    src/JobSystem.cpp
    src/Logger.cpp
    src/ostream.cpp
    src/Panic.cpp
    src/Path.cpp
    src/sstream.cpp
    src/Utf.cpp
)

set(PRIVATE_HDRS
    include/private/utils/Logger.h
    include/private/utils/ostream.h
    include/private/utils/sstream.h
)

# ==================================================================================================
# Platform Sources
# ==================================================================================================
if (ANDROID)
  list(APPEND SRCS
      src/android/Systrace.cpp
      src/linux/Path.cpp
  )
endif ()

if (APPLE)
  # mac or ios task queue implementation
  list(APPEND SRCS
      src/darwin/Path.mm
      src/darwin/Systrace.cpp
  )
endif ()

if (LINUX)
  list(APPEND SRCS
      src/linux/Path.cpp
  )
endif ()

if (WIN32)
  list(APPEND SRCS
      src/win32/Path.cpp
  )
endif ()

# ==================================================================================================
# Includes & target definition
# ==================================================================================================
include_directories(${PUBLIC_HDR_DIR} ${PRIVATE_HDR_DIR})

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

if (WIN32)
  # Needed for shlwapi.h (GetModuleFileName)
  target_link_libraries(${TARGET} PUBLIC Shlwapi)
endif ()

# `JobSystem.cpp` is the only source here that uses `std::thread`, and on every platform whose C++
# runtime does not fold the thread primitives into libc it needs the thread library named explicitly.
# `find_package(Threads)` rather than a bare `pthread`, because the answer differs between glibc
# versions and between desktop and Android, and a wrong guess is a link error in every consumer rather
# than in this target.
if (NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(${TARGET} PUBLIC $<BUILD_INTERFACE:Threads::Threads>)
endif ()

# ==================================================================================================
# Installation
# ==================================================================================================
install(TARGETS ${TARGET} EXPORT CoralUITargets ARCHIVE DESTINATION lib)

# ==================================================================================================
# Test
# ==================================================================================================
# This section existed with nothing under it. Its absence is directly connected to the bug that filled the
# hole in `SRCS` above: a module with no tests cannot notice that one of its own source files is not being
# compiled, because it never compiles anything.
if (CORAL_BUILD_TESTS)

  enable_testing()
  add_subdirectory(tests)

endif ()
