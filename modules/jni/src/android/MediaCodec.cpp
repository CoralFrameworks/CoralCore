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

#include <core/jni/android/MediaCodec.h>
#include <core/jni/android/MediaFormat.h>
#include <core/jni/android/Surface.h>

#include <sys/system_properties.h>

#include <iostream>
#include <mutex>
#include <utility>

namespace jni {

namespace {

uint32_t sApiLevel = 0;
std::once_flag sApiLevelOnceFlag;

int getApiLevel() {
  std::call_once(sApiLevelOnceFlag, []() {
    char sdkVersion[PROP_VALUE_MAX];
    __system_property_get("ro.build.version.sdk", sdkVersion);
    sApiLevel = atoi(sdkVersion);
  });
  return sApiLevel;
}

} // namespace anonymous

std::unique_ptr<MediaCodec>
MediaCodec::createDecoderByType(const std::string &type) {
  auto mediaCodec = std::make_unique<MediaCodec>();
  mediaCodec->createDecoder(type);
  return mediaCodec;
}

std::unique_ptr<MediaCodec>
MediaCodec::createEncoderByType(const std::string &type) {
  auto mediaCodec = std::make_unique<MediaCodec>();
  mediaCodec->createEncoder(type);
  return mediaCodec;
}

std::unique_ptr<MediaCodec>
MediaCodec::createByCodecName(const std::string &codeName) {
  auto mediaCodec = std::make_unique<MediaCodec>();
  mediaCodec->createByName(codeName);
  return mediaCodec;
}

void MediaCodec::createDecoder(const std::string &mimeType) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);
  bind(env);
  auto *obj = env->CallStaticObjectMethod(
      mediacodec_class.obj(), create_decoder_by_type_id,
      JniHelper::ConvertString(env, mimeType));
  mediacodec = ScopedJavaLocalRef<jobject>(env, obj);
  if (codec_buffer_info.is_null()) {
    auto *info = env->NewObject(bufferInfo_class.obj(), buffer_info_init_id);
    auto bufferInfoRef = ScopedJavaLocalRef<jobject>(env, info);
    codec_buffer_info = bufferInfoRef;
  }
}

void MediaCodec::createEncoder(const std::string &mimeType) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);
  bind(env);
  auto *obj = env->CallStaticObjectMethod(
      mediacodec_class.obj(), create_encoder_by_type_id,
      JniHelper::ConvertString(env, mimeType));
  auto codecRef = ScopedJavaLocalRef<jobject>(env, obj);
  mediacodec = codecRef;
  if (codec_buffer_info.is_null()) {
    auto *info = env->NewObject(bufferInfo_class.obj(), buffer_info_init_id);
    auto bufferInfoRef = ScopedJavaLocalRef<jobject>(env, info);
    codec_buffer_info = bufferInfoRef;
  }
}

void MediaCodec::createByName(const std::string &codeName) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr) {
    return;
  }
  JvmEnv::handleException(env);
  bind(env);
  auto *obj = env->CallStaticObjectMethod(
      mediacodec_class.obj(), create_by_codec_name_id,
      JniHelper::ConvertString(env, codeName));
  mediacodec = ScopedJavaLocalRef<jobject>(env, obj);
  if (codec_buffer_info.is_null()) {
    auto *info = env->NewObject(bufferInfo_class.obj(), buffer_info_init_id);
    codec_buffer_info = ScopedJavaLocalRef<jobject>(env, info);
  }
}

std::string MediaCodec::getName() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return {};
  }

  JvmEnv::handleException(env);
  auto name = (jstring) env->CallObjectMethod(mediacodec.obj(), get_name_id);
  if (name != nullptr) {
    auto result = JniHelper::ConvertUTFString(env, name);
    env->DeleteLocalRef(name);
    return result;
  }
  return {};
}

void MediaCodec::release() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }

  JvmEnv::handleException(env);
  env->CallVoidMethod(mediacodec.obj(), release_id);
}

void MediaCodec::configure(const MediaFormat *format, void *surface, void *crypto, int flags) {
  if (format == nullptr) {
    return;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }

  JvmEnv::handleException(env);
  env->CallVoidMethod(mediacodec.obj(),
                      configure_id,
                      format->obj(),
                      (jobject) surface,
                      crypto,
                      flags);
}

std::unique_ptr<JniSurface> MediaCodec::createInputSurface() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return {};
  }

  JvmEnv::handleException(env);
  auto *surface =
      env->CallObjectMethod(mediacodec.obj(), create_input_surface_id);
  if (surface == nullptr) {
    return {};
  }
  return JniSurface::fromObject(env, surface);
}

void MediaCodec::start() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }
  JvmEnv::handleException(env);
  env->CallVoidMethod(mediacodec.obj(), start_id);
}

void MediaCodec::stop() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }
  JvmEnv::handleException(env);

  env->CallVoidMethod(mediacodec.obj(), stop_id);
}

void MediaCodec::flush() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }
  JvmEnv::handleException(env);

  env->CallVoidMethod(mediacodec.obj(), flush_id);
}

int MediaCodec::dequeueInputBuffer(int64_t timeoutUs) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return -1;
  }
  JvmEnv::handleException(env);
  auto inputIndex =
      env->CallIntMethod(mediacodec.obj(), dequeue_input_buffer_id, timeoutUs);
  JvmEnv::handleException(env);
  return inputIndex;
}

void MediaCodec::queueInputBuffer(int index, int offset, int size, int64_t presentationTimeUs,
                                  int flags) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }
  JvmEnv::handleException(env);

  env->CallVoidMethod(mediacodec.obj(), queue_input_buffer_id, (jint) index, (jint) offset,
                      (jint) size, presentationTimeUs, flags);
}

int MediaCodec::dequeueOutputBuffer(BufferInfo *info, int64_t timeoutUs) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return INFO_TRY_AGAIN_LATER;
  }
  JvmEnv::handleException(env);

  auto ret = env->CallIntMethod(mediacodec.obj(), dequeue_output_buffer_id,
                                codec_buffer_info.obj(), timeoutUs);
  if (ret >= 0) {
    JvmEnv::handleException(env);
    info->flags = env->GetIntField(codec_buffer_info.obj(), flags_id);
    info->offset = env->GetIntField(codec_buffer_info.obj(), offset_id);
    info->presentationTimeUs =
        env->GetLongField(codec_buffer_info.obj(), presentation_time_us_id);
    info->size = env->GetIntField(codec_buffer_info.obj(), size_id);
  }
  return ret;
}

void MediaCodec::releaseOutputBuffer(int index, bool render) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }
  JvmEnv::handleException(env);

  env->CallVoidMethod(mediacodec.obj(), release_output_buffer_id, (jint) index,
                      (jboolean) render);
}

void MediaCodec::releaseOutputBuffer(int index, int64_t renderTimestampNs) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }
  JvmEnv::handleException(env);

  env->CallVoidMethod(mediacodec.obj(), release_output_buffer_at_time_id,
                      (jint) index, renderTimestampNs);
}

void MediaCodec::signalEndOfInputStream() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }
  JvmEnv::handleException(env);

  env->CallVoidMethod(mediacodec.obj(), signal_end_of_input_stream_id);
}

void MediaCodec::setVideoScalingMode(int mode) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return;
  }
  JvmEnv::handleException(env);

  env->CallVoidMethod(mediacodec.obj(), set_video_scaling_mode_id, mode);
}

void MediaCodec::setOutputSurface(jobject surface) {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null() || surface == nullptr) {
    return;
  }
  JvmEnv::handleException(env);
  if (getApiLevel() >= __ANDROID_API_M__) {
    env->CallVoidMethod(mediacodec.obj(), set_output_surface_id, surface);
  }
}

std::unique_ptr<JniMediaFormat> MediaCodec::getOutputFormat() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return {};
  }
  JvmEnv::handleException(env);

  auto mediaFormat =
      env->CallObjectMethod(mediacodec.obj(), get_output_format_id);
  if (mediaFormat == nullptr) {
    return {};
  }
  return JniMediaFormat::create(ScopedJavaLocalRef<jobject>(env, mediaFormat));
}

uint8_t *MediaCodec::getInputBuffer(int index, size_t *out_size) {
  uint8_t *ret = nullptr;
  jobject bufferObj = nullptr;
  if (index < 0) {
    return nullptr;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return nullptr;
  }
  JvmEnv::handleException(env);

  if (getApiLevel() >= __ANDROID_API_L__) {
    bufferObj = env->CallObjectMethod(mediacodec.obj(), get_input_buffer_id,
                                      (jint) index);
    if (bufferObj == nullptr) {
      return nullptr;
    }
    auto resultBuffer =
        static_cast<uint8_t *>(env->GetDirectBufferAddress(bufferObj));
    if (resultBuffer != nullptr) {
      ret = resultBuffer;
      *out_size = env->GetDirectBufferCapacity(bufferObj);
    } else {
      auto byteBuffer_class = env->FindClass("java/nio/ByteBuffer");
      auto byteBuffer_array =
          env->GetMethodID(byteBuffer_class, "array", "()[B");
      auto byteArray =
          (jbyteArray) env->CallObjectMethod(bufferObj, byteBuffer_array);
      if (byteArray != nullptr) {
        ret = (uint8_t *) (env->GetByteArrayElements(byteArray, nullptr));
        if (out_size) {
          *out_size = env->GetArrayLength(byteArray);
        }
      } else {
        ret = nullptr;
      }
    }
  } else {
    if (input_buffers.is_null()) {
      auto *buffers =
          env->CallObjectMethod(mediacodec.obj(), get_input_buffers_id);
      auto buffersRef = ScopedJavaLocalRef<jobject>(env, buffers);
      input_buffers = buffersRef;
    }
    if (!input_buffers.is_null()) {
      bufferObj =
          env->GetObjectArrayElement((jobjectArray) input_buffers.obj(), index);
      if (bufferObj == nullptr) {
        return nullptr;
      }
      auto resultBuffer =
          static_cast<uint8_t *>(env->GetDirectBufferAddress(bufferObj));
      if (resultBuffer != nullptr) {
        ret = resultBuffer;
        *out_size = env->GetDirectBufferCapacity(bufferObj);
      } else {
        auto byteBuffer_class = env->FindClass("java/nio/ByteBuffer");
        auto byteBuffer_array =
            env->GetMethodID(byteBuffer_class, "array", "()[B");
        auto byteArray =
            (jbyteArray) env->CallObjectMethod(bufferObj, byteBuffer_array);
        if (byteArray != nullptr) {
          ret = (uint8_t *) (env->GetByteArrayElements(byteArray, nullptr));
          if (out_size) {
            *out_size = env->GetArrayLength(byteArray);
          }
        }
      }
    }
  }

  return ret;
}

std::unique_ptr<uint8_t[]> MediaCodec::getOutputBuffer(int index, size_t *out_size) {
  if (index < 0) {
    return nullptr;
  }
  auto *env = JvmEnv::getThreadEnvironment();
  if (env == nullptr || mediacodec.is_null()) {
    return nullptr;
  }
  JvmEnv::handleException(env);

  std::unique_ptr<uint8_t[]> outputBuffer = nullptr;

  if (getApiLevel() >= __ANDROID_API_L__) {
    auto bufferObj = env->CallObjectMethod(mediacodec.obj(),
                                           get_output_buffer_id, (jint) index);
    if (bufferObj != nullptr) {
      auto resultBuffer =
          static_cast<uint8_t *>(env->GetDirectBufferAddress(bufferObj));
      if (resultBuffer != nullptr) {
        auto outputSize = env->GetDirectBufferCapacity(bufferObj);
        if (outputSize > 0) {
          outputBuffer = std::make_unique<uint8_t[]>(outputSize);
          memcpy(outputBuffer.get(), resultBuffer, outputSize);
        }
        if (out_size) {
          *out_size = outputSize;
        }
      } else {
        auto byteBuffer_class = env->FindClass("java/nio/ByteBuffer");
        auto byteBuffer_array =
            env->GetMethodID(byteBuffer_class, "array", "()[B");
        auto byteArray =
            (jbyteArray) env->CallObjectMethod(bufferObj, byteBuffer_array);
        if (byteArray != nullptr) {
          auto arrayBuffer =
              (uint8_t *) (env->GetByteArrayElements(byteArray, nullptr));
          auto outputSize = env->GetArrayLength(byteArray);
          outputBuffer = std::make_unique<uint8_t[]>(outputSize);
          memcpy(outputBuffer.get(), arrayBuffer, outputSize);
          if (out_size) {
            *out_size = outputSize;
          }
        }
      }
    } else {
      std::cerr << "can't not find output buffer object: " << index << std::endl;
    }
  } else {
    if (output_buffers.is_null()) {
      auto *buffers =
          env->CallObjectMethod(mediacodec.obj(), get_output_buffers_id);
      auto buffersRef = ScopedJavaLocalRef<jobject>(env, buffers);
      output_buffers = buffersRef;
    }
    if (!output_buffers.is_null()) {
      auto bufferObj =
          env->GetObjectArrayElement((jobjectArray) output_buffers.obj(), index);
      if (bufferObj) {
        auto resultBuffer = env->GetDirectBufferAddress(bufferObj);
        if (resultBuffer != nullptr) {
          auto outputSize = env->GetDirectBufferCapacity(bufferObj);
          if (outputSize > 0) {
            outputBuffer = std::make_unique<uint8_t[]>(outputSize);
            memcpy(outputBuffer.get(), resultBuffer, outputSize);
          }
          if (out_size) {
            *out_size = outputSize;
          }
        } else {
          auto byteBuffer_class = env->FindClass("java/nio/ByteBuffer");
          auto byteBuffer_array =
              env->GetMethodID(byteBuffer_class, "array", "()[B");
          auto byteArray =
              (jbyteArray) env->CallObjectMethod(bufferObj, byteBuffer_array);
          if (byteArray != nullptr) {
            auto arrayBuffer =
                (uint8_t *) (env->GetByteArrayElements(byteArray, nullptr));
            auto outputSize = env->GetArrayLength(byteArray);
            outputBuffer = std::make_unique<uint8_t[]>(outputSize);
            memcpy(outputBuffer.get(), arrayBuffer, outputSize);
            if (out_size) {
              *out_size = outputSize;
            }
          }
        }
      } else {
        std::cerr << "can't not find output buffer object:" << index << std::endl;
      }
    }
  }

  return outputBuffer;
}

void MediaCodec::bind(JNIEnv *env) {

  if (mediacodec_class.is_null()) {
    auto clazzRef = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/media/MediaCodec"));
    mediacodec_class = clazzRef;
  }

  create_by_codec_name_id =
      env->GetStaticMethodID(mediacodec_class.obj(), "createByCodecName",
          "(Ljava/lang/String;)Landroid/media/MediaCodec;");
  create_decoder_by_type_id =
      env->GetStaticMethodID(mediacodec_class.obj(), "createDecoderByType",
          "(Ljava/lang/String;)Landroid/media/MediaCodec;");
  create_encoder_by_type_id =
      env->GetStaticMethodID(mediacodec_class.obj(), "createEncoderByType",
          "(Ljava/lang/String;)Landroid/media/MediaCodec;");

  get_name_id = env->GetMethodID(mediacodec_class.obj(), "getName",
      "()Ljava/lang/String;");

  configure_id = env->GetMethodID(mediacodec_class.obj(), "configure",
      "(Landroid/media/MediaFormat;Landroid/view/"
      "Surface;Landroid/media/MediaCrypto;I)V");
  create_input_surface_id = env->GetMethodID(
      mediacodec_class.obj(), "createInputSurface", "()Landroid/view/Surface;");
  start_id = env->GetMethodID(mediacodec_class.obj(), "start", "()V");
  flush_id = env->GetMethodID(mediacodec_class.obj(), "flush", "()V");
  stop_id = env->GetMethodID(mediacodec_class.obj(), "stop", "()V");
  release_id = env->GetMethodID(mediacodec_class.obj(), "release", "()V");

  get_output_format_id =
      env->GetMethodID(mediacodec_class.obj(), "getOutputFormat",
                       "()Landroid/media/MediaFormat;");

  dequeue_input_buffer_id =
      env->GetMethodID(mediacodec_class.obj(), "dequeueInputBuffer", "(J)I");
  queue_input_buffer_id =
      env->GetMethodID(mediacodec_class.obj(), "queueInputBuffer", "(IIIJI)V");
  get_input_buffer_id = env->GetMethodID(
      mediacodec_class.obj(), "getInputBuffer", "(I)Ljava/nio/ByteBuffer;");
  get_input_buffers_id = env->GetMethodID(
      mediacodec_class.obj(), "getInputBuffers", "()[Ljava/nio/ByteBuffer;");

  dequeue_output_buffer_id =
      env->GetMethodID(mediacodec_class.obj(), "dequeueOutputBuffer",
                       "(Landroid/media/MediaCodec$BufferInfo;J)I");
  get_output_buffer_id = env->GetMethodID(
      mediacodec_class.obj(), "getOutputBuffer", "(I)Ljava/nio/ByteBuffer;");
  get_output_buffers_id = env->GetMethodID(
      mediacodec_class.obj(), "getOutputBuffers", "()[Ljava/nio/ByteBuffer;");
  release_output_buffer_id =
      env->GetMethodID(mediacodec_class.obj(), "releaseOutputBuffer", "(IZ)V");
  release_output_buffer_at_time_id =
      env->GetMethodID(mediacodec_class.obj(), "releaseOutputBuffer", "(IJ)V");

  signal_end_of_input_stream_id =
      env->GetMethodID(mediacodec_class.obj(), "signalEndOfInputStream", "()V");

  set_video_scaling_mode_id =
      env->GetMethodID(mediacodec_class.obj(), "setVideoScalingMode", "(I)V");
  if (getApiLevel() >= __ANDROID_API_M__) {
    set_output_surface_id =
        env->GetMethodID(mediacodec_class.obj(), "setOutputSurface", "(Landroid/view/Surface;)V");
  } else {
    set_output_surface_id = nullptr;
  }

  // media codec buffer info
  if (bufferInfo_class.is_null()) {
    auto clazzRef = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/media/MediaCodec$BufferInfo"));
    bufferInfo_class = clazzRef;
  }
  buffer_info_init_id = env->GetMethodID(bufferInfo_class.obj(), "<init>", "()V");
  flags_id = env->GetFieldID(bufferInfo_class.obj(), "flags", "I");
  offset_id = env->GetFieldID(bufferInfo_class.obj(), "offset", "I");
  presentation_time_us_id = env->GetFieldID(bufferInfo_class.obj(), "presentationTimeUs", "J");
  size_id = env->GetFieldID(bufferInfo_class.obj(), "size", "I");
}

} // namespace jni
