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

#ifndef CORAL_JNI_ANDROID_SURFACE_TEXTURE_H
#define CORAL_JNI_ANDROID_SURFACE_TEXTURE_H

#include <core/jni/JniScopedRef.h>

#include <cstdint>
#include <memory>

namespace jni {

class Surface;

/**
 * The jni wrapper of `android.graphics.SurfaceTexture`.
 *
 * ## What a `SurfaceTexture` is for
 *
 * It is the platform's connection between a producer that renders into a `Surface` and a consumer that
 * samples the result as an OpenGL external texture. The producer -- a decoder, a camera -- writes frames into
 * the surface; `updateTexImage` makes the oldest queued frame current in the texture the object was created
 * with; `getTransformMatrix` reports the crop and orientation to apply when that texture is sampled.
 *
 * The texture name has to be a `GL_TEXTURE_EXTERNAL_OES` name created on the thread's *current* GL context:
 * the object is bound to that context, and `detachFromGLContext` (or `release`) has to run while it is still
 * alive, which is what `release` here does.
 */
class SurfaceTexture {
public:
  ~SurfaceTexture();

  SurfaceTexture(const SurfaceTexture&) = delete;
  SurfaceTexture& operator=(const SurfaceTexture&) = delete;

  /**
   * Creates a `SurfaceTexture` over @p texName.
   *
   * @param env the calling thread's environment.
   * @param texName a `GL_TEXTURE_EXTERNAL_OES` name, current on this thread's GL context.
   * @return the wrapper, or null when the object could not be created.
   */
  static std::unique_ptr<SurfaceTexture> create(JNIEnv* env, int texName);

  /**
   * Creates a `SurfaceTexture` that is not bound to any GL context yet.
   *
   * This is what lets a producer start before the renderer exists: the surface exists from the moment the
   * object is made, so a decoder can be configured and can fill the queue, and `attachToGLContext` binds the
   * object to the renderer's texture when that thread is ready. The frames queued in between are waiting in
   * the queue at the first `updateTexImage`.
   *
   * API 26 and later only: the constructor that takes no texture name was added there.
   *
   * @param singleBufferMode whether the queue keeps one buffer, which trades throughput for the latency of a
   *                         frame being visible. The decoder path wants several buffers in flight, so it passes
   *                         false.
   */
  static std::unique_ptr<SurfaceTexture> createDetached(JNIEnv* env, bool singleBufferMode);

  /**
   * Binds the object to @p texName, which has to be a `GL_TEXTURE_EXTERNAL_OES` name of the calling thread's
   * current GL context.
   *
   * @return false when the object could not be attached, which leaves it detached.
   */
  bool attachToGLContext(JNIEnv* env, int texName);

  /** Unbinds the object from its GL context; the frames in the queue stay queued. */
  bool detachFromGLContext(JNIEnv* env);

  /** @return true when the object is bound to a GL context, so its frames can be sampled. */
  [[nodiscard]] bool isAttached() const;

  /** @return the `SurfaceTexture` object, borrowed; null once released. */
  [[nodiscard]] void* get() const;

  /** @return the producer's `Surface`, which is what a decoder is configured with. */
  [[nodiscard]] std::unique_ptr<Surface> getSurface(JNIEnv* env);

  /** The size the producer renders at; without it the surface uses the producer's default. */
  void setDefaultBufferSize(JNIEnv* env, int width, int height);

  /**
   * Makes the oldest queued frame the current content of the texture.
   *
   * @return false when there was no frame to acquire, which is the normal answer before the first one
   *         arrives.
   */
  bool updateTexImage(JNIEnv* env);

  /** The 4x4 matrix to apply to the texture coordinates of a frame read through this object. */
  void getTransformMatrix(JNIEnv* env, float matrix[16]) const;

  /** The presentation timestamp of the current frame, in nanoseconds, or 0. */
  [[nodiscard]] int64_t getTimestamp(JNIEnv* env) const;

  /** Detaches the object from the GL context and releases it; safe to call twice. */
  void release(JNIEnv* env);

  [[nodiscard]] bool isValid() const;

private:
  SurfaceTexture() = default;

  /** Resolves the class and its methods; @return false when one of them is missing. */
  bool bind(JNIEnv* env);

  jmethodID surfaceTexture_updateTexImage = nullptr;
  jmethodID surfaceTexture_getTransformMatrix = nullptr;
  jmethodID surfaceTexture_getTimestamp = nullptr;
  jmethodID surfaceTexture_setDefaultBufferSize = nullptr;
  jmethodID surfaceTexture_detachFromGLContext = nullptr;
  jmethodID surfaceTexture_attachToGLContext = nullptr;
  jmethodID surfaceTexture_release = nullptr;
  ScopedJavaGlobalRef<jclass> surfaceTexture_class;
  ScopedJavaGlobalRef<jobject> surfaceTexture;
  /** Whether the object is bound to a GL context; a detached one has no texture to sample. */
  bool mAttached = false;
};

} // namespace jni

using JniSurfaceTexture = jni::SurfaceTexture;

#endif // CORAL_JNI_ANDROID_SURFACE_TEXTURE_H


