/*
 * Copyright (C) 2025 The Coral Project
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

#ifndef CORAL_UTILS_BASE64_H
#define CORAL_UTILS_BASE64_H

#include <cstddef>
#include <cstdint>

namespace utils {

class Base64 {
public:
  enum Error : uint8_t {
    kNoError,
    kPadError,
    kBadCharError
  };

  /**
   * Base64 encodes src into dst.
   * Normally this is called once with 'dst' nullptr to get the required size, then again with an
   * allocated 'dst' pointer to do the actual encoding.
   *
   * @param src    src data to encode
   * @param length length of the src
   * @param dst    nullptr or a pointer to a buffer large enough to receive the result
   * @param encode nullptr for default encoding or a pointer to at least 65 chars.
   *               encode[64] will be used as the pad character.
   *               Encodings other than the default encoding cannot be decoded.
   * @return the required length of dst for encoding.
   */
  static size_t encode(const void* src, size_t length, void* dst, const char* encode = nullptr);

  /**
   * Returns the length of the buffer that needs to be allocated to encode srcDataLength bytes.
   */
  static size_t encodedSize(const size_t srcDataLength) {
    // Take the floor of division by 3 to find the number of groups that need to be encoded.
    // Each group takes 4 bytes to be represented in base64.
    return ((srcDataLength + 2) / 3) * 4;
  }

  /**
   * Base64 decodes src into dst.
   * Normally this is called once with 'dst' nullptr to get the required size, then again with an
   * allocated 'dst' pointer to do the actual encoding.
   *
   * @param src       src data to decode
   * @param srcLength length of the src
   * @param dst       nullptr or a pointer to a buffer large enough to receive the result
   * @param dstLength assigned the length dst is required to be. Must not be nullptr.
   * @return
   */
  static Error decode(const void* src, size_t srcLength, void* dst, size_t* dstLength);
};

} // namespace utils

#endif //CORAL_UTILS_BASE64_H
