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

#include <core/jni/android/MediaExtractor.h>

namespace jni {

MediaExtractor::MediaExtractor() {
  auto *env = JvmEnv::getThreadEnvironment();
  if (env != nullptr) {
    bind(env);
  }
}

MediaExtractor::~MediaExtractor() = default;

void MediaExtractor::bind(JNIEnv *env) {
  JvmEnv::handleException(env);
  auto clazzRef = ScopedJavaLocalRef<jclass>(env, env->FindClass("android/media/MediaExtractor"));
  Extractor_class = clazzRef;
  MediaExtractor_init = env->GetMethodID(Extractor_class.obj(), "<init>", "()V");
  MediaExtractor_setDataSource = env->GetMethodID(Extractor_class.obj(), "setDataSource", "(Ljava/lang/String;)V");
  MediaExtractor_release = env->GetMethodID(Extractor_class.obj(), "release", "()V");
  MediaExtractor_getTrackCount = env->GetMethodID(Extractor_class.obj(), "getTrackCount", "()I");
  MediaExtractor_getTrackFormat =
      env->GetMethodID(Extractor_class.obj(), "getTrackFormat", "(I)Landroid/media/MediaFormat;");
  MediaExtractor_selectTrack = env->GetMethodID(Extractor_class.obj(), "selectTrack", "(I)V");
  MediaExtractor_unselectTrack = env->GetMethodID(Extractor_class.obj(), "unselectTrack", "(I)V");
  MediaExtractor_seekTo = env->GetMethodID(Extractor_class.obj(), "seekTo", "(JI)V");
  MediaExtractor_advance = env->GetMethodID(Extractor_class.obj(), "advance", "()Z");
  MediaExtractor_readSampleData =
      env->GetMethodID(Extractor_class.obj(), "readSampleData", "(Ljava/nio/ByteBuffer;I)I");
  MediaExtractor_getSampleTrackIndex = env->GetMethodID(Extractor_class.obj(), "getSampleTrackIndex", "()I");
  MediaExtractor_getSampleTime = env->GetMethodID(Extractor_class.obj(), "getSampleTime", "()J");
  MediaExtractor_getSampleSize = env->GetMethodID(Extractor_class.obj(), "getSampleSize", "()J");
  MediaExtractor_getSampleFlags = env->GetMethodID(Extractor_class.obj(), "getSampleFlags", "()I");
  MediaExtractor_getCachedDuration = env->GetMethodID(Extractor_class.obj(), "getCachedDuration", "()J");
  MediaExtractor_hasCacheReachedEndOfStream =
      env->GetMethodID(Extractor_class.obj(), "hasCacheReachedEndOfStream", "()Z");

  // new object
  auto obj = ScopedJavaLocalRef<jobject>(env, env->NewObject(Extractor_class.obj(), MediaExtractor_init));
  extractor = obj;
}

void MediaExtractor::setDataSource(std::string path) {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return;
  }
  env->CallVoidMethod(extractor.obj(), MediaExtractor_setDataSource, JniHelper::ConvertString(env, path));
  JvmEnv::handleException(env);
}

void MediaExtractor::release() {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return;
  }
  env->CallVoidMethod(extractor.obj(), MediaExtractor_release);
  JvmEnv::handleException(env);
}

int MediaExtractor::getTrackCount() {
  auto env = JvmEnv::get().getEnvironment();
  if (!env || extractor.is_null()) {
    return 0;
  }
  int trackCount = env->CallIntMethod(extractor.obj(), MediaExtractor_getTrackCount);
  JvmEnv::handleException(env);
  return trackCount;
}

std::unique_ptr<MediaFormat> MediaExtractor::getTrackFormat(int index) {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return {};
  }
  auto format = env->CallObjectMethod(extractor.obj(), MediaExtractor_getTrackFormat, index);
  if (!format) {
    return {};
  }
  return JniMediaFormat::create(ScopedJavaLocalRef<jobject>(env, format));
}

void MediaExtractor::selectTrack(int index) {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return;
  }
  env->CallVoidMethod(extractor.obj(), MediaExtractor_selectTrack, index);
  JvmEnv::handleException(env);
}

void MediaExtractor::unselectTrack(int index) {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return;
  }
  env->CallVoidMethod(extractor.obj(), MediaExtractor_unselectTrack, index);
  JvmEnv::handleException(env);
}

void MediaExtractor::seekTo(long timeUs, SeekMode mode) {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return;
  }
  env->CallVoidMethod(extractor.obj(), MediaExtractor_seekTo, timeUs, (int)mode);
  JvmEnv::handleException(env);
}

bool MediaExtractor::advance() {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return false;
  }
  auto result = env->CallBooleanMethod(extractor.obj(), MediaExtractor_advance);
  JvmEnv::handleException(env);
  return result;
}

int MediaExtractor::readSampleData(const std::unique_ptr<ByteBuffer>& byteBuffer, int offset) {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return -1;
  }
  auto result = env->CallIntMethod(extractor.obj(), MediaExtractor_readSampleData, byteBuffer->obj(), offset);
  JvmEnv::handleException(env);
  return result;
}

int MediaExtractor::getSampleTrackIndex() {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return -1;
  }
  auto result = env->CallIntMethod(extractor.obj(), MediaExtractor_getSampleTrackIndex);
  JvmEnv::handleException(env);
  return result;
}

long MediaExtractor::getSampleTime() {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return -1;
  }
  auto result = env->CallLongMethod(extractor.obj(), MediaExtractor_getSampleTime);
  JvmEnv::handleException(env);
  return result;
}

long MediaExtractor::getSampleSize() {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return -1;
  }
  auto result = env->CallLongMethod(extractor.obj(), MediaExtractor_getSampleSize);
  JvmEnv::handleException(env);
  return result;
}

MediaExtractor::SampleFlag MediaExtractor::getSampleFlags() {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return MediaExtractor::SAMPLE_FLAG_SYNC;
  }
  auto result = env->CallIntMethod(extractor.obj(), MediaExtractor_getSampleFlags);
  JvmEnv::handleException(env);
  return (SampleFlag)result;
}

long MediaExtractor::getCachedDuration() {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return -1;
  }
  auto result = env->CallLongMethod(extractor.obj(), MediaExtractor_getCachedDuration);
  JvmEnv::handleException(env);
  return result;
}

bool MediaExtractor::hasCacheReachedEndOfStream() {
  auto env = JvmEnv::getThreadEnvironment();
  if (!env || extractor.is_null()) {
    return false;
  }
  auto result = env->CallBooleanMethod(extractor.obj(), MediaExtractor_hasCacheReachedEndOfStream);
  JvmEnv::handleException(env);
  return result;
}

} // namespace jni
