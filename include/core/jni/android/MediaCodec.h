/*
 * Copyright (C) 2022 The Graphifx Project
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

#ifndef CORAL_JNI_ANDROID_MEDIA_CODEC_H
#define CORAL_JNI_ANDROID_MEDIA_CODEC_H

#include <core/jni/JniHelper.h>
#include <core/jni/JniScopedRef.h>
#include <core/jni/android/Surface.h>

#include <memory>
#include <string>

namespace jni {

class MediaFormat;

/**
 * The jni wrapper of android.media.MediaCodec
 */
class MediaCodec final {
public:
  typedef struct {
    int32_t offset;
    int32_t size;
    int64_t presentationTimeUs;
    uint32_t flags;
  } BufferInfo;

  /**
   * This indicates that the (encoded) buffer marked as such contains
   * the data for a key frame.
   */
  static constexpr int BUFFER_FLAG_KEY_FRAME = 1;

  /**
   * This indicated that the buffer marked as such contains codec
   * initialization / codec specific data instead of media data.
   */
  static constexpr int BUFFER_FLAG_CODEC_CONFIG = 2;

  /**
   * This signals the end of stream, i.e. no buffers will be available
   * after this, unless of course, {@link #flush} follows.
   */
  static constexpr int BUFFER_FLAG_END_OF_STREAM = 4;

  /**
   * This indicates that the buffer only contains part of a frame,
   * and the decoder should batch the data until a buffer without
   * this flag appears before decoding the frame.
   */
  static constexpr int BUFFER_FLAG_PARTIAL_FRAME = 8;

  /**
   * If this codec is to be used as an encoder, pass this flag.
   */
  static constexpr int CONFIGURE_FLAG_ENCODE = 1;

  /**
   * If a non-negative timeout had been specified in the call
   * to {@link ::dequeueOutputBuffer}, indicates that the call timed out.
   */
  static constexpr int INFO_TRY_AGAIN_LATER = -1;

  /**
   * The output format has changed, subsequent data will follow the new
   * format. {@link ::getOutputFormat()} returns the new format.  Note, that
   * you can also use the new {@link ::getOutputFormat(int)} method to
   * get the format for a specific output buffer.  This frees you from
   * having to track output format changes.
   */
  static constexpr int INFO_OUTPUT_FORMAT_CHANGED = -2;

  /**
   * The output buffers have changed, the client must refer to the new
   * set of output buffers returned by {@link ::getOutputBuffers} from
   * this point on.
   *
   * @deprecated This return value can be ignored as {@link
   * ::getOutputBuffers} has been deprecated.  Client should
   * request a current buffer using on of the get-buffer or
   * get-image methods each time one has been dequeued.
   */
  static constexpr int INFO_OUTPUT_BUFFERS_CHANGED = -3;

  /**
   * The content is scaled to the surface dimensions
   */
  static constexpr int VIDEO_SCALING_MODE_SCALE_TO_FIT = 1;

  /**
   * The content is scaled, maintaining its aspect ratio, the whole
   * surface area is used, content may be cropped.
   * <p class=note>
   * This mode is only suitable for content with 1:1 pixel aspect ratio as you
   * cannot configure the pixel aspect ratio for a {@link Surface}. <p
   * class=note> As of {@link android.os.Build.VERSION_CODES#N} release, this
   * mode may not work if the video is {@linkplain MediaFormat#KEY_ROTATION
   * rotated} by 90 or 270 degrees.
   */
  static constexpr int VIDEO_SCALING_MODE_SCALE_TO_FIT_WITH_CROPPING = 2;

  /**
   * Instantiate a decoder supporting input data of the given mime type.
   *
   * @param type The mime type of the input data.
   */
  static std::unique_ptr<MediaCodec> createDecoderByType(const std::string& type);

  /**
   * Instantiate an encoder supporting output data of the given mime type.
   *
   * @param type The desired mime type of the output data.
   */
  static std::unique_ptr<MediaCodec> createEncoderByType(const std::string& type);

  /**
   * If you know the exact name of the component you want to instantiate
   * use this method to instantiate it. Use with caution.
   *
   * @param codeName  The name of the codec to be instantiated.
   */
  static std::unique_ptr<MediaCodec> createByCodecName(const std::string& codeName);

  /**
   * Get the component name. If the codec was created by createDecoderByType
   * or createEncoderByType, what component is chosen is not known beforehand.
   */
  std::string getName();

  /**
   * Free up resources used by the codec instance.
   *
   * Make sure you call this when you're done to free up any opened
   * component instance instead of relying on the garbage collector
   * to do this for you at some point in the future.
   */
  void release();

  /**
   * Configures a component.
   *
   * @param format The format of the input data (decoder) or the desired
   *               format of the output data (encoder). Passing {@code null}
   *               as {@code format} is equivalent to passing an
   *               {@link MediaFormat#MediaFormat an empty mediaformat}.
   * @param surface Specify a surface on which to render the output of this
   *                decoder. Pass {@code null} as {@code surface} if the
   *                codec does not generate raw video output (e.g. not a video
   *                decoder) and/or if you want to configure the codec for
   *                {@link ByteBuffer} output.
   * @param crypto Specify a crypto object to facilitate secure decryption
   *                of the media data. Pass {@code null} as {@code crypto} for
   *                non-secure codecs.
   *                Please note that {@link MediaCodec} does NOT take ownership
   *                of the {@link MediaCrypto} object; it is the application's
   *                responsibility to properly cleanup the {@link MediaCrypto}
   * object when not in use.
   * @param flags Specify {@link ::CONFIGURE_FLAG_ENCODE} to configure the
   *                component as an encoder.
   */
  void configure(const MediaFormat* format, void* surface, void* crypto, int flags);

  /**
   * Requests a Surface to use as the input to an encoder, in place of input
   * buffers.  This may only be called after {@link #configure} and before
   * {@link #start}. <p> The application is responsible for calling release() on
   * the Surface when done.
   */
  std::unique_ptr<JniSurface> createInputSurface();

  /**
   * After successfully configuring the component, call {@code start}.
   * <p>
   * Call {@code start} also if the codec is configured in asynchronous mode,
   * and it has just been flushed, to resume requesting input buffers.
   */
  void start();

  /**
   * Finish the decode/encode session, note that the codec instance
   * remains active and ready to be {@link ::start}ed again.
   * To ensure that it is available to other client call {@link ::release}
   * and don't just rely on garbage collection to eventually do this for you.
   */
  void stop();

  /**
   * Flush both input and output ports of the component.
   */
  void flush();

  /**
   * After filling a range of the input buffer at the specified index
   * submit it to the component. Once an input buffer is queued to
   * the codec, it MUST NOT be used until it is later retrieved by
   * {@link #getInputBuffer} in response to a {@link ::dequeueInputBuffer}
   * return value or a {@link Callback#onInputBufferAvailable}
   * callback.
   *
   * @param index The index of a client-owned input buffer previously returned
   *              in a call to {@link ::dequeueInputBuffer}.
   * @param offset The byte offset into the input buffer at which the data
   * starts.
   * @param size The number of bytes of valid input data.
   * @param presentationTimeUs The presentation timestamp in microseconds for
   * this buffer. This is normally the media time at which this buffer should be
   * presented (rendered). When using an output surface, this will be propagated
   * as the {@link SurfaceTexture#getTimestamp timestamp} for the frame (after
   *                           conversion to nanoseconds).
   * @param flags A bitmask of flags
   *              {@link ::BUFFER_FLAG_CODEC_CONFIG} and {@link
   * ::BUFFER_FLAG_END_OF_STREAM}. While not prohibited, most codecs do not use
   * the
   *              {@link ::BUFFER_FLAG_KEY_FRAME} flag for input buffers.
   */
  void queueInputBuffer(int index, int offset, int size, int64_t presentationTimeUs, int flags);

  /**
   * Returns the index of an input buffer to be filled with valid data
   * or -1 if no such buffer is currently available.
   * This method will return immediately if timeoutUs == 0, wait indefinitely
   * for the availability of an input buffer if timeoutUs &lt; 0 or wait up
   * to "timeoutUs" microseconds if timeoutUs &gt; 0.
   *
   * @param timeoutUs The timeout in microseconds, a negative timeout indicates
   * "infinite".
   */
  int dequeueInputBuffer(int64_t timeoutUs);

  /**
   * Dequeue an output buffer, block at most "timeoutUs" microseconds.
   * Returns the index of an output buffer that has been successfully
   * decoded or one of the INFO_* constants below.
   *
   * @param info  Will be filled with buffer meta data.
   * @param timeoutUs The timeout in microseconds, a negative timeout indicates
   * "infinite".
   */
  int dequeueOutputBuffer(BufferInfo *info, int64_t timeoutUs);

  /**
   * If you are done with a buffer, use this call to return the buffer to
   * the codec. If you previously specified a surface when configuring this
   * video decoder you can optionally render the buffer.
   *
   * @param index The index of a client-owned output buffer previously returned
   *              from a call to {@link ::dequeueOutputBuffer}.
   * @param render If a valid surface was specified when configuring the codec,
   *               passing true renders this output buffer to the surface.
   */
  void releaseOutputBuffer(int index, bool render);

  /**
   * If you are done with a buffer, use this call to update its surface
   * timestamp and return it to the codec to render it on the output surface. If
   * you have not specified an output surface when configuring this video codec,
   * this call will simply return the buffer to the codec.<p>
   *
   * The timestamp may have special meaning depending on the destination
   * surface.
   *
   * @param index The index of a client-owned output buffer previously returned
   *              from a call to {@link ::dequeueOutputBuffer}.
   * @param renderTimestampNs The timestamp to associate with this buffer when
   *              it is sent to the Surface.
   */
  void releaseOutputBuffer(int index, int64_t renderTimestampNs);

  /**
   * Signals end-of-stream on input.  Equivalent to submitting an empty buffer
   * with
   * {@link ::BUFFER_FLAG_END_OF_STREAM} set.  This may only be used with
   * encoders receiving input from a Surface created by {@link
   * ::createInputSurface}.
   */
  void signalEndOfInputStream();

  /**
   * If a surface has been specified in a previous call to {@link #configure}
   * specifies the scaling mode to use. The default is "scale to fit".
   * <p class=note>
   * The scaling mode may be reset to the <strong>default</strong> each time an
   * {@link #INFO_OUTPUT_BUFFERS_CHANGED} event is received from the codec;
   * therefore, the client must call this method after every buffer change event
   * (and before the first output buffer is released for rendering) to ensure
   * consistent scaling mode. <p class=note> Since the {@link
   * #INFO_OUTPUT_BUFFERS_CHANGED} event is deprecated, this can also be done
   * after each {@link #INFO_OUTPUT_FORMAT_CHANGED} event.
   */
  void setVideoScalingMode(int mode);

  /**
   * Dynamically sets the output surface of a codec.
   *
   * This can only be used if the codec was configured with an output surface.
   * The new output surface should have a compatible usage type to the original
   * output surface. E.g. codecs may not support switching from a SurfaceTexture
   * (GPU readable) output to ImageReader (software readable) output.
   * @param surface the output surface to use. It must not be null.
   */
  void setOutputSurface(jobject surface);

  /**
   * Call this after dequeueOutputBuffer signals a format change by returning
   * {@link #INFO_OUTPUT_FORMAT_CHANGED}.
   * You can also call this after {@link #configure} returns
   * successfully to get the output format initially configured
   * for the codec.  Do this to determine what optional
   * configuration parameters were supported by the codec.
   */
  std::unique_ptr<MediaFormat> getOutputFormat();

  /**
   * Returns a cleared, writable ByteBuffer object for a dequeued input buffer
   * index to contain the input data. After calling this method any ByteBuffer
   * or Image object previously returned for the same input index MUST no longer
   * be used.
   *
   * @param index The index of a client-owned input buffer previously returned
   * from a call to dequeueInputBuffer, or received via an
   * onInputBufferAvailable callback.
   */
  uint8_t *getInputBuffer(int index, size_t *out_size);

  std::unique_ptr<uint8_t[]> getOutputBuffer(int index, size_t *out_size);

private:
  void bind(JNIEnv *env);

  void createDecoder(const std::string &mimeType);

  void createEncoder(const std::string &mimeType);

  void createByName(const std::string &codeName);

  jmethodID create_by_codec_name_id;
  jmethodID create_decoder_by_type_id;
  jmethodID create_encoder_by_type_id;

  jmethodID get_name_id;

  jmethodID configure_id;
  jmethodID create_input_surface_id;
  jmethodID start_id;
  jmethodID flush_id;
  jmethodID stop_id;
  jmethodID release_id;

  jmethodID get_output_format_id;

  jmethodID dequeue_input_buffer_id;
  jmethodID queue_input_buffer_id;
  jmethodID get_input_buffer_id;
  jmethodID get_input_buffers_id;

  jmethodID dequeue_output_buffer_id;
  jmethodID get_output_buffer_id;
  jmethodID get_output_buffers_id;
  jmethodID release_output_buffer_id;
  jmethodID release_output_buffer_at_time_id;

  jmethodID signal_end_of_input_stream_id;
  jmethodID set_video_scaling_mode_id;

  jmethodID set_output_surface_id;

  //!< android.media.MediaCodec$BufferInfo wrapper
  ScopedJavaGlobalRef<jclass> bufferInfo_class;
  jmethodID buffer_info_init_id;
  jfieldID flags_id;
  jfieldID offset_id;
  jfieldID presentation_time_us_id;
  jfieldID size_id;

  ScopedJavaGlobalRef<jclass> mediacodec_class;
  ScopedJavaGlobalRef<jobject> mediacodec;
  ScopedJavaGlobalRef<jobject> codec_buffer_info;

  //!< only available when sdkInt is less than 21
  ScopedJavaGlobalRef<jobject> input_buffers;
  ScopedJavaGlobalRef<jobject> output_buffers;
};
} // namespace jni

using JniMediaCodec = jni::MediaCodec;

#endif // CORAL_JNI_ANDROID_MEDIA_CODEC_H
