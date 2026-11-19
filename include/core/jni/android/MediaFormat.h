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

#ifndef CORAL_JNI_ANDROID_MEDIA_FORMAT_H
#define CORAL_JNI_ANDROID_MEDIA_FORMAT_H

#include <core/jni/android/ByteBuffer.h>

#include <core/jni/JniScopedRef.h>

#include <memory>
#include <string>

namespace jni {

/**
 * The jni wrapper of the android.media.MediaFormat
 */
class MediaFormat final {
public:
  static constexpr auto KEY_MIME = "mime";
  static constexpr auto KEY_SAMPLE_RATE = "sample-rate";
  static constexpr auto KEY_CHANNEL_COUNT = "channel-count";
  static constexpr auto KEY_CHANNEL_MASK = "channel-mask";
  static constexpr auto KEY_AAC_PROFILE = "aac-profile";
  static constexpr auto KEY_WIDTH = "width";
  static constexpr auto KEY_HEIGHT = "height";
  static constexpr auto KEY_BIT_RATE = "bitrate";
  static constexpr auto KEY_BITRATE_MODE = "bitrate-mode";
  static constexpr auto KEY_MAX_INPUT_SIZE = "max-input-size";
  static constexpr auto KEY_FRAME_RATE = "frame-rate";
  static constexpr auto KEY_PCM_ENCODING = "pcm-encoding";
  static constexpr auto KEY_I_FRAME_INTERVAL = "i-frame-interval";
  static constexpr auto KEY_TRACK_ID = "track-id";
  static constexpr auto KEY_MAX_B_FRAMES = "max-bframes";
  static constexpr auto KEY_PROFILE = "profile";
  static constexpr auto KEY_LEVEL = "level";
  static constexpr auto KEY_LANGUAGE = "language";
  static constexpr auto KEY_DURATION = "durationUs";
  static constexpr auto KEY_IS_ADTS = "is-adts";
  static constexpr auto KEY_FRAME_COUNT = "frame-count";
  static constexpr auto KEY_ROTATION = "rotation-degrees";
  static constexpr auto KEY_COLOR_FORMAT = "color-format";

  static std::unique_ptr<MediaFormat> create();
  static std::unique_ptr<MediaFormat> create(const ScopedJavaLocalRef<jobject>& object);

  MediaFormat();
  explicit MediaFormat(const ScopedJavaLocalRef<jobject>& object) noexcept;
  virtual ~MediaFormat() = default;

  std::string toString();

  bool getInteger(const std::string &key, int32_t *out);
  bool getLong(const std::string &key, int64_t *out);
  bool getFloat(const std::string &key, float *out);
  std::unique_ptr<JniByteBuffer> getByteBuffer(const std::string &key);
  bool getString(const std::string &key, std::string *out);

  void setInteger(const std::string &key, int32_t value);
  void setLong(const std::string &key, int64_t value);
  void setFloat(const std::string &key, float value);
  void setByteBuffer(const std::string &key, void *data, size_t size);
  void setString(const std::string &key, const std::string &value);

  [[nodiscard]] jobject obj() const { return object.obj(); }

private:
  void bind();

  bool prepared;

  jmethodID init_id;

  jmethodID contains_key_id;

  jmethodID get_integer_id;
  jmethodID get_long_id;
  jmethodID get_float_id;
  jmethodID get_bytebuffer_id;
  jmethodID get_string_id;

  jmethodID set_integer_id;
  jmethodID set_long_id;
  jmethodID set_float_id;
  jmethodID set_bytebuffer_id;
  jmethodID set_string_id;

  jmethodID to_string_id;

  jfieldID mMap;

  ScopedJavaGlobalRef<jclass> clazz;
  ScopedJavaGlobalRef<jobject> object;
};

} // namespace jni

using JniMediaFormat = jni::MediaFormat;

#endif // CORAL_JNI_ANDROID_MEDIA_FORMAT_H
