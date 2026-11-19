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

#ifndef CORAL_JNI_ANDROID_AUDIO_FORMAT_H
#define CORAL_JNI_ANDROID_AUDIO_FORMAT_H

namespace jni {

class AudioFormat final {
public:
  //!< AudioFormat::ENCODING_PCM_16BIT
  static constexpr int ENCODING_PCM_16BIT = 2;
  //!< AudioFormat::ENCODING_PCM_FLOAT
  static constexpr int ENCODING_PCM_FLOAT = 4;
  //!< AudioFormat::ENCODING_PCM_24BIT_PACKED
  static constexpr int ENCODING_PCM_24BIT_PACKED = 21;
  //!< AudioFormat::ENCODING_PCM_32BIT
  static constexpr int ENCODING_PCM_32BIT = 22;

  // Same as the definition of android.media.AudioFormat.java
  static constexpr int CHANNEL_IN_DEFAULT = 1;
  static constexpr int CHANNEL_IN_LEFT = 0x4;
  static constexpr int CHANNEL_IN_RIGHT = 0x8;
  static constexpr int CHANNEL_IN_FRONT = 0x10;
  static constexpr int CHANNEL_IN_BACK = 0x20;
  static constexpr int CHANNEL_IN_LEFT_PROCESSED = 0x40;
  static constexpr int CHANNEL_IN_RIGHT_PROCESSED = 0x80;
  static constexpr int CHANNEL_IN_FRONT_PROCESSED = 0x100;
  static constexpr int CHANNEL_IN_BACK_PROCESSED = 0x200;
  static constexpr int CHANNEL_IN_PRESSURE = 0x400;
  static constexpr int CHANNEL_IN_X_AXIS = 0x800;
  static constexpr int CHANNEL_IN_Y_AXIS = 0x1000;
  static constexpr int CHANNEL_IN_Z_AXIS = 0x2000;
  static constexpr int CHANNEL_IN_VOICE_UPLINK = 0x4000;
  static constexpr int CHANNEL_IN_VOICE_DNLINK = 0x8000;
  static constexpr int CHANNEL_IN_MONO = CHANNEL_IN_FRONT;
  static constexpr int CHANNEL_IN_STEREO = (CHANNEL_IN_LEFT | CHANNEL_IN_RIGHT);

  static constexpr int CHANNEL_IN_FRONT_BACK = CHANNEL_IN_FRONT | CHANNEL_IN_BACK;

private:
  AudioFormat() = default;
  ~AudioFormat() = default;
};

} // namespace jni

using JniAudioFormat = jni::AudioFormat;

#endif // CORAL_JNI_ANDROID_AUDIO_FORMAT_H
