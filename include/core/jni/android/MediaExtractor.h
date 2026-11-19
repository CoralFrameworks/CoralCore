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

#ifndef CORAL_JNI_ANDROID_MEDIA_EXTRACTOR_H
#define CORAL_JNI_ANDROID_MEDIA_EXTRACTOR_H

#include <core/jni/JniHelper.h>
#include <core/jni/JniScopedRef.h>
#include <core/jni/android/ByteBuffer.h>
#include <core/jni/android/MediaFormat.h>

#include <memory>
#include <string>

namespace jni {

/**
 * The jni wrapper of the android.media.MediaExtractor
 */
class MediaExtractor final {
public:
  MediaExtractor();

  ~MediaExtractor();

  /**
   * Sets the data source (file-path or http URL) to use.
   *
   * @param path the path of the file, or the http URL of the stream.
   */
  void setDataSource(std::string path);

  /**
   * Make sure you call this when you're done to free up any resources
   * instead of relying on the garbage collector to do this for you at
   * some point in the future.
   */
  void release();

  /**
   * Count the number of tracks found in the data source.
   */
  int getTrackCount();

  /**
   * Get the track format at the specified index.
   */
  std::unique_ptr<MediaFormat> getTrackFormat(int index);

  /**
   * Subsequent calls to {@link readSampleData}, {@link getSampleTrackIndex} and {@link getSampleTime} only
   * retrieve information for the subset of tracks selected.
   *
   * Selecting the same track multiple times has no effect, the track is only selected once.
   */
  void selectTrack(int index);

  /**
   * Subsequent calls to {@link readSampleData}, {@link getSampleTrackIndex} and {@link getSampleTime} only
   * retrieve information for the subset of tracks selected.
   */
  void unselectTrack(int index);

  enum SeekMode : uint8_t {
    /**
     * If possible, seek to a sync sample at or before the specified time
     */
    SEEK_TO_PREVIOUS_SYNC = 0,
    /**
     * If possible, seek to a sync sample at or after the specified time
     */
    SEEK_TO_NEXT_SYNC = 1,
    /**
     * If possible, seek to the sync sample closest to the specified time
     */
    SEEK_TO_CLOSEST_SYNC = 2,
  };

  /**
   * All selected tracks seek near the requested time according to the specified mode.
   */
  void seekTo(long timeUs, SeekMode mode);

  /**
   * Advance to the next sample. Returns false if no more sample data is available (end of stream).
   *
   * When extracting a local file, the behaviors of {@link advance} and
   * {@link readSampleData} are undefined in presence of concurrent
   * writes to the same local file; more specifically, end of stream
   * could be signalled earlier than expected.
   */
  bool advance();

  /**
   * Retrieve the current encoded sample and store it in the byte buffer starting at the given offset.
   *
   * @param byteBuffer the destination byte buffer
   * @param offset the offset to read.
   *
   * @return the sample size (or -1 if no more samples are available).
   */
  int readSampleData(const std::unique_ptr<ByteBuffer>& byteBuffer, int offset = 0);

  /**
   * Returns the track index the current sample originates from (or -1
   * if no more samples are available)
   */
  int getSampleTrackIndex();

  /**
   * Returns the current sample's presentation time in microseconds.
   * or -1 if no more samples are available.
   */
  long getSampleTime();

  /**
   * Returns the size of the current sample in bytes or -1 if no more
   * samples are available.
   */
  long getSampleSize();

  enum SampleFlag : uint8_t {
    /**
     * The sample is a sync sample (or in {@link MediaCodec}'s terminology it is a key frame.)
     */
    SAMPLE_FLAG_SYNC = 1,
    /**
     * The sample is (at least partially) encrypted, see also the documentation
     * for {@link android.media.MediaCodec#queueSecureInputBuffer}
     */
    SAMPLE_FLAG_ENCRYPTED = 2,
    /**
     * This indicates that the buffer only contains part of a frame,
     * and the decoder should batch the data until a buffer without
     * this flag appears before decoding the frame.
     *
     * @see MediaCodec#BUFFER_FLAG_PARTIAL_FRAME
     */
    SAMPLE_FLAG_PARTIAL_FRAME = 4,
  };

  SampleFlag getSampleFlags();

  /**
   * Returns an estimate of how much data is presently cached in memory
   * expressed in microseconds. Returns -1 if that information is unavailable
   * or not applicable (no cache).
   */
  long getCachedDuration();

  /**
   * Returns true iff we are caching data and the cache has reached the
   * end of the data stream (for now, a future seek may of course restart
   * the fetching of data).
   * This API only returns a meaningful result if {@link #getCachedDuration}
   * indicates the presence of a cache, i.e. does NOT return -1.
   */
  bool hasCacheReachedEndOfStream();

private:

  void bind(JNIEnv* env);

  ScopedJavaGlobalRef<jclass> Extractor_class;
  ScopedJavaGlobalRef<jobject> extractor;

  jmethodID MediaExtractor_init;
  jmethodID MediaExtractor_setDataSource;
  jmethodID MediaExtractor_release;
  jmethodID MediaExtractor_getTrackCount;
  jmethodID MediaExtractor_getTrackFormat;
  jmethodID MediaExtractor_selectTrack;
  jmethodID MediaExtractor_unselectTrack;
  jmethodID MediaExtractor_seekTo;
  jmethodID MediaExtractor_advance;
  jmethodID MediaExtractor_readSampleData;
  jmethodID MediaExtractor_getSampleTrackIndex;
  jmethodID MediaExtractor_getSampleTime;
  jmethodID MediaExtractor_getSampleSize;
  jmethodID MediaExtractor_getSampleFlags;
  jmethodID MediaExtractor_getCachedDuration;
  jmethodID MediaExtractor_hasCacheReachedEndOfStream;
};

} // namespace jni

using JniMediaExtractor = jni::MediaExtractor;

#endif //CORAL_JNI_ANDROID_MEDIA_EXTRACTOR_H
