/*
 * Copyright (C) 2022 The Coral Project
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

#ifndef CORAL_JNI_ANDROID_MEDIA_MUXER_H
#define CORAL_JNI_ANDROID_MEDIA_MUXER_H

#include <core/jni/JniHelper.h>
#include <core/jni/JniScopedRef.h>
#include <core/jni/android/ByteBuffer.h>
#include <core/jni/android/MediaCodec.h>
#include <core/jni/android/MediaFormat.h>

#include <memory>
#include <string>

namespace jni {

/**
 * The jni wrapper of the android.media.MediaMuxer
 */
class MediaMuxer final {
public:
  enum OutputFormat : uint8_t {
    /** MPEG4 media file format*/
    MUXER_OUTPUT_MPEG_4 = 0,
    /** WEBM media file format*/
    MUXER_OUTPUT_WEBM = 1,
    /** 3GPP media file format*/
    MUXER_OUTPUT_3GPP = 2,
    /** HEIF media file format*/
    MUXER_OUTPUT_HEIF = 3,
    /** Ogg media file format*/
    MUXER_OUTPUT_OGG = 4,
  };

  ~MediaMuxer();

  /**
   * Creates a MediaMuxer.
   */
  static std::unique_ptr<MediaMuxer> CreateWithUrl(std::string path, OutputFormat format);

  /**
   * Sets the orientation hint for output video playback.
   * @param degrees the angle to be rotated clockwise in degrees.
   * The supported angles are 0, 90, 180, and 270 degrees.
   */
  void setOrientationHint(int degrees);

  /**
   * Set and store the geodata (latitude and longitude) in the output file.
   *
   * @param latitude   Latitude in degrees. Its value must be in the range [-90, 90].
   * @param longitude  Longitude in degrees. Its value must be in the range [-180, 180].
   */
  void setLocation(float latitude, float longitude);

  /**
   * Starts the muxer.
   */
  void start();

  /**
   * Stops the muxer.
   */
  void stop();

  /**
   * Adds a track with the specified format.
   *
   * @param format The media format for the track.  This must not be an empty MediaFormat.
   *
   * @return The track index for this newly added track, and it should be used in the {@link writeSampleData}.
   */
  int addTrack(const std::unique_ptr<MediaFormat>& format);

  void writeSampleData(int trackIndex, const std::unique_ptr<ByteBuffer>& byteBuffer, MediaCodec::BufferInfo bufferInfo);

  /**
   * Make sure you call this when you're done to free up any resources
   * instead of relying on the garbage collector to do this for you at
   * some point in the future.
   */
  void release();

private:
  MediaMuxer();

  void bind(JNIEnv* env);

  void setOutputPath(std::string path, OutputFormat format);

  ScopedJavaGlobalRef<jclass> MediaMuxer_class;
  ScopedJavaGlobalRef<jobject> mediaMuxerObj;

  //!< android.media.MediaMuxer wrapper
  jmethodID MediaMuxer_init;
  jmethodID MediaMuxer_setOrientationHint;
  jmethodID MediaMuxer_setLocation;
  jmethodID MediaMuxer_start;
  jmethodID MediaMuxer_stop;
  jmethodID MediaMuxer_addTrack;
  jmethodID MediaMuxer_writeSampleData;
  jmethodID MediaMuxer_release;

  //!< android.media.MediaCodec$BufferInfo wrapper
  ScopedJavaGlobalRef<jclass> BufferInfo_class;
  ScopedJavaGlobalRef<jobject> bufferInfoObj;
  jmethodID BufferInfo_init;
  jfieldID BufferInfo_flags;
  jfieldID BufferInfo_offset;
  jfieldID BufferInfo_presentationTimeUs;
  jfieldID BufferInfo_size;
};

} // namespace jni

using JniMediaMuxer = jni::MediaMuxer;

#endif //CORAL_JNI_ANDROID_MEDIA_MUXER_H
