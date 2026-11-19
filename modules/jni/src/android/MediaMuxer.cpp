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

#include <core/jni/android/MediaMuxer.h>

namespace jni {

MediaMuxer::MediaMuxer() {
  auto env = JvmEnv::get().getEnvironment();
  if (env) {
    bind(env);
  }
}

MediaMuxer::~MediaMuxer() = default;

void MediaMuxer::bind(JNIEnv *env) {
  //!< android.media.MediaMuxer wrapper
  if (MediaMuxer_class.is_null()) {
    auto clazzRef = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/media/MediaMuxer"));
    MediaMuxer_class = clazzRef;
    MediaMuxer_init = env->GetMethodID(MediaMuxer_class.obj(), "<init>", "(Ljava/lang/String;I)V");
    MediaMuxer_setOrientationHint = env->GetMethodID(MediaMuxer_class.obj(), "setOrientationHint", "(I)V");
    MediaMuxer_setLocation = env->GetMethodID(MediaMuxer_class.obj(), "setLocation", "(FF)V");
    MediaMuxer_start = env->GetMethodID(MediaMuxer_class.obj(), "start", "()V");
    MediaMuxer_stop = env->GetMethodID(MediaMuxer_class.obj(), "stop", "()V");
    MediaMuxer_addTrack = env->GetMethodID(MediaMuxer_class.obj(), "addTrack", "(Landroid/media/MediaFormat;)I");
    MediaMuxer_writeSampleData =
        env->GetMethodID(MediaMuxer_class.obj(),
            "writeSampleData",
            "(ILjava/nio/ByteBuffer;Landroid/media/MediaCodec$BufferInfo;)V");
    MediaMuxer_release = env->GetMethodID(MediaMuxer_class.obj(), "release", "()V");
  }

  //!< android.media.MediaCodec$BufferInfo wrapper
  if (BufferInfo_class.is_null()) {
    auto bufferInfoRef = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/media/MediaCodec$BufferInfo"));
    BufferInfo_class = bufferInfoRef;
    BufferInfo_init = env->GetMethodID(BufferInfo_class.obj(), "<init>", "()V");
    BufferInfo_flags = env->GetFieldID(BufferInfo_class.obj(), "flags", "I");
    BufferInfo_offset = env->GetFieldID(BufferInfo_class.obj(), "offset", "I");
    BufferInfo_presentationTimeUs = env->GetFieldID(BufferInfo_class.obj(), "presentationTimeUs", "J");
    BufferInfo_size = env->GetFieldID(BufferInfo_class.obj(), "size", "I");
  }
}

std::unique_ptr<MediaMuxer> MediaMuxer::CreateWithUrl(std::string path, OutputFormat format) {
  auto muxer = std::unique_ptr<MediaMuxer>(new MediaMuxer());
  muxer->setOutputPath(path, format);
  return muxer;
}

void MediaMuxer::setOutputPath(std::string path, OutputFormat format) {
  auto env = JvmEnv::get().getEnvironment();
  if (!env) {
    return;
  }
  if (!mediaMuxerObj.is_null()) {
    release();
  }
  auto obj = ScopedJavaLocalRef<jobject>(env,
      env->NewObject(MediaMuxer_class.obj(), MediaMuxer_init, JniHelper::ConvertString(env, path), format));
  mediaMuxerObj = obj;

  if (bufferInfoObj.is_null()) {
    auto *info = env->NewObject(BufferInfo_class.obj(), BufferInfo_init);
    auto bufferInfoRef = ScopedJavaLocalRef<jobject>(env, info);
    bufferInfoObj = bufferInfoRef;
  }
}

void MediaMuxer::setOrientationHint(int degrees) {
  auto env = JvmEnv::get().getEnvironment();
  if (!env || mediaMuxerObj.is_null()) {
    return;
  }
  env->CallVoidMethod(mediaMuxerObj.obj(), MediaMuxer_setOrientationHint, degrees);
  JvmEnv::handleException(env);
}

void MediaMuxer::setLocation(float latitude, float longitude) {
  auto env = JvmEnv::get().getEnvironment();
  if (!env || mediaMuxerObj.is_null()) {
    return;
  }
  env->CallVoidMethod(mediaMuxerObj.obj(), MediaMuxer_setLocation, latitude, longitude);
  JvmEnv::handleException(env);
}

void MediaMuxer::start() {
  auto env = JvmEnv::get().getEnvironment();
  if (!env || mediaMuxerObj.is_null()) {
    return;
  }
  env->CallVoidMethod(mediaMuxerObj.obj(), MediaMuxer_start);
  JvmEnv::handleException(env);
}

void MediaMuxer::stop() {
  auto env = JvmEnv::get().getEnvironment();
  if (!env || mediaMuxerObj.is_null()) {
    return;
  }
  env->CallVoidMethod(mediaMuxerObj.obj(), MediaMuxer_stop);
  JvmEnv::handleException(env);
}

int MediaMuxer::addTrack(const std::unique_ptr<MediaFormat>& format) {
  auto env = JvmEnv::get().getEnvironment();
  if (!env || mediaMuxerObj.is_null() || !format) {
    return -1;
  }
  int track = env->CallIntMethod(mediaMuxerObj.obj(), MediaMuxer_addTrack, format->obj());
  JvmEnv::handleException(env);
  return track;
}

void MediaMuxer::writeSampleData(int trackIndex, const std::unique_ptr<ByteBuffer>& byteBuffer,
    MediaCodec::BufferInfo bufferInfo) {
  auto env = JvmEnv::get().getEnvironment();
  if (!env || mediaMuxerObj.is_null() || !byteBuffer) {
    return;
  }

  env->SetIntField(bufferInfoObj.obj(), BufferInfo_flags, bufferInfo.flags);
  env->SetIntField(bufferInfoObj.obj(), BufferInfo_offset, bufferInfo.offset);
  env->SetLongField(bufferInfoObj.obj(), BufferInfo_presentationTimeUs, bufferInfo.presentationTimeUs);
  env->SetIntField(bufferInfoObj.obj(), BufferInfo_size, bufferInfo.size);

  env->CallVoidMethod(mediaMuxerObj.obj(),
      MediaMuxer_writeSampleData,
      trackIndex,
      byteBuffer->obj(),
      bufferInfoObj.obj());
  JvmEnv::handleException(env);
}

void MediaMuxer::release() {
  auto env = JvmEnv::get().getEnvironment();
  if (!env || mediaMuxerObj.is_null()) {
    return;
  }
  env->CallVoidMethod(mediaMuxerObj.obj(), MediaMuxer_release);
  JvmEnv::handleException(env);
}

} // namespace jni
