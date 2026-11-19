cmake_minimum_required(VERSION 3.19)
set(TARGET jni)
set(PUBLIC_HDR_DIR ${CORALCORE_INCLUDE_DIR})

# ==================================================================================================
# Sources and headers
# ==================================================================================================

set(PUBLIC_HDRS
    ${PUBLIC_HDR_DIR}/core/jni/JniHelper.h
    ${PUBLIC_HDR_DIR}/core/jni/JniScopedByteArray.h
    ${PUBLIC_HDR_DIR}/core/jni/JniScopedRef.h
    ${PUBLIC_HDR_DIR}/core/jni/JvmEnv.h
)

set(SRCS
    src/JniHelper.cpp
    src/JniScopedByteArray.cpp
    src/JvmEnv.cpp
)

if (ANDROID)
  list(APPEND PUBLIC_HDRS
      ${PUBLIC_HDR_DIR}/core/jni/android/AssetFileDescriptor.h
      ${PUBLIC_HDR_DIR}/core/jni/android/AssetManager.h
      ${PUBLIC_HDR_DIR}/core/jni/android/AudioFormat.h
      ${PUBLIC_HDR_DIR}/core/jni/android/ByteBuffer.h
      ${PUBLIC_HDR_DIR}/core/jni/android/ContentResolver.h
      ${PUBLIC_HDR_DIR}/core/jni/android/Context.h
      ${PUBLIC_HDR_DIR}/core/jni/android/FileDescriptor.h
      ${PUBLIC_HDR_DIR}/core/jni/android/InputStream.h
      ${PUBLIC_HDR_DIR}/core/jni/android/MediaCodec.h
      ${PUBLIC_HDR_DIR}/core/jni/android/MediaExtractor.h
      ${PUBLIC_HDR_DIR}/core/jni/android/MediaFormat.h
      ${PUBLIC_HDR_DIR}/core/jni/android/MediaMuxer.h
      ${PUBLIC_HDR_DIR}/core/jni/android/Resources.h
      ${PUBLIC_HDR_DIR}/core/jni/android/Surface.h
      ${PUBLIC_HDR_DIR}/core/jni/android/SurfaceTexture.h
      ${PUBLIC_HDR_DIR}/core/jni/android/Uri.h
  )
  list(APPEND SRCS
      src/android/AssetFileDescriptor.cpp
      src/android/AssetManager.cpp
      src/android/ByteBuffer.cpp
      src/android/ContentResolver.cpp
      src/android/Context.cpp
      src/android/FileDescriptor.cpp
      src/android/InputStream.cpp
      src/android/MediaCodec.cpp
      src/android/MediaExtractor.cpp
      src/android/MediaFormat.cpp
      src/android/MediaMuxer.cpp
      src/android/Resources.cpp
      src/android/Surface.cpp
      src/android/SurfaceTexture.cpp
      src/android/Uri.cpp
  )
endif ()

# ==================================================================================================
# Includes & target definition
# ==================================================================================================
include_directories(${PUBLIC_HDR_DIR})

add_library(${TARGET} STATIC ${PUBLIC_HDRS} ${SRCS})

target_include_directories(${TARGET} PUBLIC ${PUBLIC_HDR_DIR})

# The top-level CMakeLists.txt only adds this directory when JNI support is available.
# On Android, jni.h comes from the NDK. On host platforms it comes from the JDK located by
# FindJNI, which reads JAVA_HOME and defines JNI::JNI on CMake >= 3.24.
if (IS_HOST_PLATFORM)
  if (TARGET JNI::JNI)
    # PUBLIC propagates the JDK include directories to consumers, because this library exposes
    # public headers that include <jni.h>.
    target_link_libraries(${TARGET} PUBLIC JNI::JNI)
    # The SurfaceTexture wrapper reports a missing Java method instead of aborting on a null id, and that report
    # goes to the platform log.
    if (ANDROID)
      target_link_libraries(${TARGET} PRIVATE log)
    endif ()
  elseif (JNI_INCLUDE_DIRS)
    target_include_directories(${TARGET} PUBLIC ${JNI_INCLUDE_DIRS})
  else ()
    message(FATAL_ERROR "JNI include directories are not available")
  endif ()
endif ()

set_target_properties(${TARGET} PROPERTIES FOLDER Libs)

# ==================================================================================================
# Installation
# ==================================================================================================
install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

# ==================================================================================================
# Test
# ==================================================================================================



