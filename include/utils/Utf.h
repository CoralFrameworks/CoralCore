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


#ifndef CORAL_UTILS_UTF_H
#define CORAL_UTILS_UTF_H

#include <cstddef>
#include <cstdint>

namespace utils {

class Utf {
public:
  /**
   * Returns the number of Unicode codepoints of the given sequence of UTF-8 bytes.
   * If the sequence is invalid, return -1.
   *
   * @param utf8 the given sequence of aligned UTF-8 characters
   * @param byteSize the byte size of the give sequence
   * @return the number of Unicode codepoints, or -1 if the sequence is invalid.
   */
  static int getCount(const char* utf8, size_t byteSize);

  /**
   * Returns the number of Unicode codepoints of the given sequence of aligned UTF-16 characters
   * in machine-endian form. If the sequence is invalid, return -1.
   *
   * @param utf16 the given sequence of aligned UTF-16 characters
   * @param byteSize the byte size of the give sequence
   * @return the number of Unicode codepoints, or -1 if the sequence is invalid.
   */
  static int getCount(const uint16_t* utf16, size_t byteSize);

  /**
   * Returns the number of Unicode codepoints of the given sequence of aligned UTF-32 characters
   * in machine-endian form. If the sequence is invalid, return -1.
   *
   * @param utf32 the given sequence of aligned UTF-32 characters
   * @param byteSize the byte size of the give sequence
   * @return the number of Unicode codepoints, or -1 if the sequence is invalid.
   */
  static int getCount(const int32_t* utf32, size_t byteSize);

  /**
   * Given a sequence of UTF-8 bytes, return the first Unicode codepoint.
   * The pointer will be incremented to point at the next codepoint's start.
   * If invalid UTF-8 is encountered, set *ptr to end and return -1.
   */
  static int32_t nextUtf8(const char** ptr, const char* end);

  /**
   * Given a sequence of UTF-8 bytes, return the first Unicode codepoint.
   * The pointer will be incremented to point at the next codepoint's start.
   * If invalid UTF-8 is encountered, set *ptr to end and return the
   * replacement character (0xFFFD)
   */
  static int32_t nextUtf8WithReplacement(const char** ptr, const char* end);

  /**
   * Given a sequence of aligned UTF-16 characters in machine-endian form,
   * return the first Unicode codepoint.  The pointer will be incremented to
   * point at the next codepoint's start.  If invalid UTF-16 is encountered,
   * set *ptr to end and return -1.
   */
  static int32_t nextUtf16(const uint16_t** ptr, const uint16_t* end);

  /**
   * Given a sequence of aligned UTF-32 characters in machine-endian form,
   * return the first Unicode codepoint.  The pointer will be incremented to
   * point at the next codepoint's start.  If invalid UTF-32 is encountered,
   * set *ptr to end and return -1.
   */
  static int32_t nextUtf32(const int32_t** ptr, const int32_t* end);

  static constexpr unsigned kMaxBytesInUTF8Sequence = 4;

  /**
   * Convert the Unicode codepoint into UTF-8.  If `utf8` is non-null, place the
   * result in that array.  Return the number of bytes in the result.  If `utf8`
   * is null, simply return the number of bytes that would be used.  For invalid
   * Unicode codepoints, return 0.
   */
  static size_t toUtf8(int32_t uni, char utf8[kMaxBytesInUTF8Sequence] = nullptr);

  /**
   * Convert the Unicode codepoint into UTF-16.  If `utf16` is non-null, place
   * the result in that array.  Return the number of UTF-16 code units in the
   * result (1 or 2).  If `utf16` is null, simply return the number of code
   * units that would be used.  For invalid Unicode codepoints, return 0.
   */
  static size_t toUtf16(int32_t uni, uint16_t utf16[2] = nullptr);

  /**
   * Returns the number of resulting UTF16 values needed to convert the src utf8 sequence.
   * If dst is not null, it is filled with the corresponding values up to its capacity.
   * If there is an error, -1 is returned and the dst[] buffer is undefined.
   */
  static int utf8ToUtf16(uint16_t dst[], size_t dstCapacity, const char src[], size_t srcByteSize);

  /**
   * Returns the number of resulting UTF8 values needed to convert the src utf16 sequence.
   *  If dst is not null, it is filled with the corresponding values up to its capacity.
   *  If there is an error, -1 is returned and the dst[] buffer is undefined.
   */
  static int utf16ToUtf8(char dst[], size_t dstCapacity, const uint16_t src[], size_t srcByteSize);

  /**
   * Given a UTF-16 code point, returns true if it is a leading surrogate.
   * https://unicode.org/faq/utf_bom.html#utf16-2
   */
  static inline bool isLeadingSurrogateUTF16(uint16_t c) { return ((c) & 0xFC00) == 0xD800; }

  /**
   * Given a UTF-16 code point, returns true iff it is a trailing surrogate.
   * https://unicode.org/faq/utf_bom.html#utf16-2
   */
  static inline bool isTrailingSurrogateUTF16(uint16_t c) { return ((c) & 0xFC00) == 0xDC00; }

protected:
  Utf() = default;
  ~Utf() = default;

private:
  static constexpr inline int32_t left_shift(int32_t value, int32_t shift) {
    return (int32_t) ((uint32_t) value << shift);
  }

  template<typename T>
  static constexpr bool is_align2(T x) { return 0 == (x & 1); }

  template<typename T>
  static constexpr bool is_align4(T x) { return 0 == (x & 3); }

  static constexpr inline bool utf16_is_high_surrogate(uint16_t c) {
    return (c & 0xFC00) == 0xD800;
  }

  static constexpr inline bool utf16_is_low_surrogate(uint16_t c) {
    return (c & 0xFC00) == 0xDC00;
  }

  /**
   * @return -1  iff invalid UTF8 byte,
   *          0  iff UTF8 continuation byte,
   *          1  iff ASCII byte,
   *          2  iff leading byte of 2-byte sequence,
   *          3  iff leading byte of 3-byte sequence, and
   *          4  iff leading byte of 4-byte sequence.
   *   I.e.: if return value > 0, then gives length of sequence.
   */
  static int utf8_byte_type(uint8_t c);

  static bool utf8_type_is_valid_leading_byte(const int type) { return type > 0; }

  static bool utf8_byte_is_continuation(const uint8_t c) { return utf8_byte_type(c) == 0; }
};

} // namespace utils

#endif //CORAL_UTILS_UTF_H
