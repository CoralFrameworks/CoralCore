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

#include <utils/Utf.h>

namespace utils {

int Utf::utf8_byte_type(const uint8_t c) {
  if (c < 0x80) {
    return 1;
  } else if (c < 0xC0) {
    return 0;
  } else if (c >= 0xF5 || (c & 0xFE) == 0xC0) {
    // "octet values c0, c1, f5 to ff never appear"
    return -1;
  } else {
    int value = (((0xe5 << 24) >> ((unsigned)c >> 4 << 1)) & 3) + 1;
    // assert(value >= 2 && value <=4);
    return value;
  }
}

int Utf::getCount(const char* utf8, const size_t byteSize) {
  if (!utf8) {
    return -1;
  }
  int count = 0;
  const char* stop = utf8 + byteSize;
  while (utf8 < stop) {
    int type = utf8_byte_type(*(const uint8_t*)utf8);
    if (!utf8_type_is_valid_leading_byte(type) || utf8 + type > stop) {
      return -1;  // Sequence extends beyond end.
    }
    while (type-- > 1) {
      ++utf8;
      if (!utf8_byte_is_continuation(*(const uint8_t*)utf8)) {
        return -1;
      }
    }
    ++utf8;
    ++count;
  }
  return count;
}

int Utf::getCount(const uint16_t* utf16, const size_t byteSize) {
  if (!utf16 || !is_align2(intptr_t(utf16)) || !is_align2(byteSize)) {
    return -1;
  }
  const uint16_t* src = (const uint16_t*)utf16;
  const uint16_t* stop = src + (byteSize >> 1);
  int count = 0;
  while (src < stop) {
    unsigned c = *src++;
    if (utf16_is_low_surrogate(c)) {
      return -1;
    }
    if (utf16_is_high_surrogate(c)) {
      if (src >= stop) {
        return -1;
      }
      c = *src++;
      if (!utf16_is_low_surrogate(c)) {
        return -1;
      }
    }
    count += 1;
  }
  return count;
}

int Utf::getCount(const int32_t* utf32, const size_t byteSize) {
  int value = byteSize >> 2;
  if (!is_align4(intptr_t(utf32)) || !is_align4(byteSize) || value <= 0) {
    return -1;
  }
  const uint32_t kInvalidUnicharMask = 0xFF000000;    // unichar fits in 24 bits
  const uint32_t* ptr = (const uint32_t*)utf32;
  const uint32_t* stop = ptr + value;
  while (ptr < stop) {
    if (*ptr & kInvalidUnicharMask) {
      return -1;
    }
    ptr += 1;
  }
  return value;
}

template<typename T>
static int32_t next_fail(const T** ptr, const T* end) {
  *ptr = end;
  return -1;
}

int32_t Utf::nextUtf8(const char** ptr, const char* end) {
  if (!ptr || !end) {
    return -1;
  }
  const uint8_t* p = (const uint8_t*)*ptr;
  if (!p || p >= (const uint8_t*)end) {
    return next_fail(ptr, end);
  }
  int c = *p;
  int hic = c << 24;

  if (!utf8_type_is_valid_leading_byte(utf8_byte_type(c))) {
    return next_fail(ptr, end);
  }
  if (hic < 0) {
    uint32_t mask = (uint32_t)~0x3F;
    hic = left_shift(hic, 1);
    do {
      ++p;
      if (p >= (const uint8_t*)end) {
        return next_fail(ptr, end);
      }
      // check before reading off end of array.
      uint8_t nextByte = *p;
      if (!utf8_byte_is_continuation(nextByte)) {
        return next_fail(ptr, end);
      }
      c = (c << 6) | (nextByte & 0x3F);
      mask <<= 5;
    } while ((hic = left_shift(hic, 1)) < 0);
    c &= ~mask;
  }
  *ptr = (const char*)p + 1;
  return c;
}

int32_t Utf::nextUtf8WithReplacement(const char** ptr, const char* end) {
  auto val = nextUtf8(ptr, end);
  return val < 0 ? 0xFFFD : val;
}

int32_t Utf::nextUtf16(const uint16_t** ptr, const uint16_t* end) {
  if (!ptr || !end) {
    return -1;
  }
  const uint16_t* src = *ptr;
  if (!src || src + 1 > end || !is_align2(intptr_t(src))) {
    return next_fail(ptr, end);
  }
  uint16_t c = *src++;
  int32_t result = c;
  if (utf16_is_low_surrogate(c)) {
    return next_fail(ptr, end);  // srcPtr should never point at low surrogate.
  }
  if (utf16_is_high_surrogate(c)) {
    if (src + 1 > end) {
      return next_fail(ptr, end);  // Truncated string.
    }
    uint16_t low = *src++;
    if (!utf16_is_low_surrogate(low)) {
      return next_fail(ptr, end);
    }
    /*
    [paraphrased from wikipedia]
    Take the high surrogate and subtract 0xD800, then multiply by 0x400.
    Take the low surrogate and subtract 0xDC00.  Add these two results
    together, and finally add 0x10000 to get the final decoded codepoint.

    unicode = (high - 0xD800) * 0x400 + low - 0xDC00 + 0x10000
    unicode = (high * 0x400) - (0xD800 * 0x400) + low - 0xDC00 + 0x10000
    unicode = (high << 10) - (0xD800 << 10) + low - 0xDC00 + 0x10000
    unicode = (high << 10) + low - ((0xD800 << 10) + 0xDC00 - 0x10000)
    */
    result = (result << 10) + (int32_t)low - ((0xD800 << 10) + 0xDC00 - 0x10000);
  }
  *ptr = src;
  return result;
}

int32_t Utf::nextUtf32(const int32_t** ptr, const int32_t* end) {
  if (!ptr || !end) {
    return -1;
  }
  const int32_t* s = *ptr;
  if (!s || s + 1 > end || !is_align4(intptr_t(s))) {
    return next_fail(ptr, end);
  }
  int32_t value = *s;
  const uint32_t kInvalidUnicharMask = 0xFF000000;    // unichar fits in 24 bits
  if (value & kInvalidUnicharMask) {
    return next_fail(ptr, end);
  }
  *ptr = s + 1;
  return value;
}

size_t Utf::toUtf8(int32_t uni, char utf8[kMaxBytesInUTF8Sequence]) {
  if ((uint32_t)uni > 0x10FFFF) {
    return 0;
  }
  if (uni <= 127) {
    if (utf8) {
      *utf8 = (char)uni;
    }
    return 1;
  }
  char tmp[4];
  char* p = tmp;
  size_t count = 1;
  while (uni > 0x7F >> count) {
    *p++ = (char)(0x80 | (uni & 0x3F));
    uni >>= 6;
    count += 1;
  }
  if (utf8) {
    p = tmp;
    utf8 += count;
    while (p < tmp + count - 1) {
      *--utf8 = *p++;
    }
    *--utf8 = (char)(~(0xFF >> count) | uni);
  }
  return count;
}

size_t Utf::toUtf16(int32_t uni, uint16_t utf16[2]) {
  if ((uint32_t)uni > 0x10FFFF) {
    return 0;
  }
  int extra = (uni > 0xFFFF);
  if (utf16) {
    if (extra) {
      utf16[0] = (uint16_t)((0xD800 - 64) + (uni >> 10));
      utf16[1] = (uint16_t)(0xDC00 | (uni & 0x3FF));
    } else {
      utf16[0] = (uint16_t)uni;
    }
  }
  return 1 + extra;
}

int Utf::utf8ToUtf16(uint16_t* dst, size_t dstCapacity, const char* src, const size_t srcByteSize) {
  if (!dst) {
    dstCapacity = 0;
  }

  int dstLength = 0;
  uint16_t* endDst = dst + dstCapacity;
  const char* endSrc = src + srcByteSize;
  while (src < endSrc) {
    auto uni = nextUtf8(&src, endSrc);
    if (uni < 0) {
      return -1;
    }

    uint16_t utf16[2];
    size_t count = toUtf16(uni, utf16);
    if (count == 0) {
      return -1;
    }
    dstLength += count;

    if (dst) {
      uint16_t* elems = utf16;
      while (dst < endDst && count > 0) {
        *dst++ = *elems++;
        count -= 1;
      }
    }
  }
  return dstLength;
}

int Utf::utf16ToUtf8(char* dst, size_t dstCapacity, const uint16_t* src, const size_t srcByteSize) {
  if (!dst) {
    dstCapacity = 0;
  }

  int dstLength = 0;
  const char* endDst = dst + dstCapacity;
  const uint16_t* endSrc = src + srcByteSize;
  while (src < endSrc) {
    auto uni = nextUtf16(&src, endSrc);
    if (uni < 0) {
      return -1;
    }

    char utf8[kMaxBytesInUTF8Sequence];
    size_t count = toUtf8(uni, utf8);
    if (count == 0) {
      return -1;
    }
    dstLength += count;

    if (dst) {
      const char* elems = utf8;
      while (dst < endDst && count > 0) {
        *dst++ = *elems++;
        count -= 1;
      }
    }
  }
  return dstLength;
}

} // namespace utils
