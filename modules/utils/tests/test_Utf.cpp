/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

/**
 * @file test_Utf.cpp
 *
 * `utils::Utf`, which converts between UTF-8, UTF-16 and UTF-32 one code point at a time.
 *
 * ## Why these tests are the first
 *
 * `src/Utf.cpp` was absent from `libs/utils/CMakeLists.txt` for as long as it has existed, so none of it had ever been
 * compiled, let alone run -- while `include/utils/Utf.h` was in `PUBLIC_HDRS` and therefore installed. Every function it
 * declares was an unresolved external for a caller, and `libs/core`'s `File.cpp` is one: its Windows path conversion
 * calls `Utf::nextUtf8` and `Utf::toUtf16`.
 *
 * So this is the first execution of any of it, and nothing here is a formality.
 *
 * ## Two tests assert behaviour that is wrong, and one asserts a disagreement
 *
 * Nothing calls the buffer conversions, so what they do is latent rather than harmful today -- but a test that smoothed
 * it over would be hiding exactly what the first real caller would hit. This file asserts the behaviour as it *is*, so
 * that correcting it changes these tests rather than silently invalidating them.
 *
 *  - `utf16ToUtf8`'s last parameter is documented as a **byte** size and used as a **unit count**.
 *  - `utf16ToUtf8` does not refuse a destination too small for the result: it writes what fits, abandons a code point
 *    mid-sequence, and returns the length it *would* have written.
 *  - `nextUtf32` accepts a surrogate and a value past U+10FFFF, one of which the encoders refuse and the other of which
 *    they accept. The module holds four different answers to "is this a code point".
 *
 * ## A C++ trap this file fell into
 *
 * `"a\xC3\x28z"` is **not** four bytes. A `\x` escape consumes as many hexadecimal digits as follow it, so `\x28z`
 * parses as one value with three hex digits and the literal is seven bytes. Four of the five failures in the first
 * version of this file were that mistake rather than anything in `Utf`. Every multi-byte sequence here is closed with a
 * string-literal break: `"a\xC3" "(z)"`.
 */

#include <utils/Utf.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace utils {
namespace {

// The sample text as explicit bytes, so that no escape can run on: "a", e-acute, the euro sign, a grinning emoji, "z".
// **Eleven** bytes and five code points -- one, two, three and four bytes for the four non-ASCII characters, plus the
// single-byte 'a' and 'z'.
//
// This constant said ten at first, which cost four failing tests: the walk stopped a byte early, the count came out four,
// and two comparisons were missing a trailing 'z'. Every one of those looked like a bug in `Utf` and was an arithmetic
// mistake here. A literal's length is worth measuring rather than counting in one's head.
//
// The adjacent-string breaks are also load-bearing, for a different reason. A `\x` escape consumes as many hexadecimal
// digits as follow it, so `"\x80z"` would be a single value with three digits rather than a byte and a letter. Closing
// the quote after every escape is what stops it.
constexpr char kSample[] = "a\xC3\xA9" "\xE2\x82\xAC" "\xF0\x9F\x98\x80" "z";
constexpr size_t kSampleBytes = 11;
constexpr int kSampleCodePoints = 5;
// The same text in UTF-16 is six *units* for five code points, because the emoji is a surrogate pair. `utf8ToUtf16`
// returns units, which is what its callers need to size a buffer -- and is also why it cannot be compared against
// `kSampleCodePoints` without the distinction being the point of the assertion.
constexpr int kSampleUtf16Units = 6;

TEST(UtfTest, EncodesTheBoundariesOfEachUtf8Length) {
  // The four lengths and the value on each side of every boundary. An encoder off by one at a boundary produces a
  // sequence that decodes to a *different* code point, which is the failure that looks like mojibake rather than a bug.
  struct Case { int32_t codePoint; const char* bytes; size_t length; };
  const Case cases[] = {
    { 0x00, "\x00", 1 },                     // one byte, the lowest
    { 0x7F, "\x7F", 1 },                     // one byte, the highest
    { 0x80, "\xC2\x80", 2 },                 // two bytes, the lowest
    { 0x7FF, "\xDF\xBF", 2 },                // two bytes, the highest
    { 0x800, "\xE0\xA0\x80", 3 },            // three bytes, the lowest
    { 0xFFFF, "\xEF\xBF\xBF", 3 },           // three bytes, the highest below the surrogates
    { 0x10000, "\xF0\x90\x80\x80", 4 },      // four bytes, the lowest
    { 0x10FFFF, "\xF4\x8F\xBF\xBF", 4 },     // four bytes, the highest -- the last code point there is
  };

  for (const Case& testCase : cases) {
    char buffer[Utf::kMaxBytesInUTF8Sequence] = {};
    const size_t length = Utf::toUtf8(testCase.codePoint, buffer);
    EXPECT_EQ(length, testCase.length) << "code point " << testCase.codePoint;
    EXPECT_EQ(std::string(buffer, length), std::string(testCase.bytes, testCase.length))
            << "code point " << testCase.codePoint;
  }

  // Above the last code point there is nothing to encode, and the answer is a refusal rather than a truncated sequence.
  char buffer[Utf::kMaxBytesInUTF8Sequence] = {};
  EXPECT_EQ(Utf::toUtf8(0x110000, buffer), 0u) << "one past the last code point";
  EXPECT_EQ(Utf::toUtf16(0x110000), 0u) << "and the same for UTF-16";
}

TEST(UtfTest, CountsCodePointsRatherThanUnits) {
  // The distinction this file is about: a UTF-8 buffer holds more bytes than code points whenever anything is non-ASCII,
  // and a UTF-16 buffer holds more units than code points whenever anything is outside the basic plane.
  EXPECT_EQ(Utf::getCount("hello", 5), 5);
  EXPECT_EQ(kSampleBytes, 11u) << "the sample is eleven bytes";
  EXPECT_EQ(Utf::getCount(kSample, kSampleBytes), kSampleCodePoints);

  // The same text in UTF-16: three one-unit code points, a surrogate pair for the emoji, and "z".
  const uint16_t utf16[] = { 0x0061, 0x00E9, 0x20AC, 0xD83D, 0xDE00, 0x007A };
  EXPECT_EQ(Utf::getCount(utf16, sizeof(utf16)), kSampleCodePoints);

  // And in UTF-32, where a unit *is* a code point, so the two counts agree.
  const int32_t utf32[] = { 0x61, 0xE9, 0x20AC, 0x1F600, 0x7A };
  EXPECT_EQ(Utf::getCount(utf32, sizeof(utf32)), kSampleCodePoints);
}

TEST(UtfTest, WalksUtf8OneCodePointAtATime) {
  // The cursor walk, which is how a decoder is used: `nextUtf8` advances past the code point it returned. The pointer
  // has to land exactly on the end -- a decoder that stops early leaves trailing input silently unread.
  const char* cursor = kSample;
  const char* const end = cursor + kSampleBytes;

  std::vector<int32_t> walked;
  while (cursor < end) {
    const int32_t codePoint = Utf::nextUtf8(&cursor, end);
    ASSERT_GE(codePoint, 0) << "valid input reported as malformed";
    walked.push_back(codePoint);
  }

  EXPECT_EQ(cursor, end) << "the walk must consume the whole buffer";
  const std::vector<int32_t> expected { 0x61, 0xE9, 0x20AC, 0x1F600, 0x7A };
  EXPECT_EQ(walked, expected);
}

TEST(UtfTest, RefusesMalformedUtf8AndParksTheCursorAtTheEnd) {
  // Each of these is something a decoder that trusts its input would read past the end of the buffer for. The return is
  // negative -- and the cursor is moved to `end` rather than left where it was, which is the convention that makes a
  // caller's `while (cursor < end)` loop terminate. A caller who retried at the same position would spin forever, so the
  // position is worth writing down.
  // A surrogate encoded as UTF-8 is *not* in this list. It was, and the loop is what found that the decoder accepts it;
  // see the separate case below, which pins that as a defect rather than a passing expectation.
  const std::vector<std::string> malformed {
    std::string("\xC3"),              // a two-byte lead with nothing after it
    std::string("\xE2\x82"),          // a three-byte lead with one byte after it
    std::string("\xF0\x9F\x98"),      // a four-byte lead with two bytes after it
    std::string("\x80"),              // a continuation byte where a lead belongs
    std::string("\xC0\x80"),          // an overlong encoding of NUL: never legal
    std::string("\xF5\x80\x80\x80"),  // 0xF5 is not a valid lead byte: beyond U+10FFFF
  };

  for (const std::string& bad : malformed) {
    const char* cursor = bad.data();
    const char* const end = cursor + bad.size();

    EXPECT_LT(Utf::nextUtf8(&cursor, end), 0) << "accepted " << bad.size() << " malformed byte(s)";
    EXPECT_EQ(cursor, end) << "the cursor must be parked at the end, so the caller's loop terminates";
  }

  // A fourth defect, and the most interesting of them: this decoder accepts a UTF-8-encoded **surrogate**.
  //
  // `ED A0 80` is structurally well-formed -- a valid three-byte lead, two proper continuation bytes -- and decodes to
  // U+D800, which is not a code point. Every conforming UTF-8 decoder rejects what the Unicode standard calls a
  // surrogate code point, and a decoder that accepts one can be used to smuggle invalid text past a validator: the bytes
  // are not valid UTF-8, but this function says they are, and a second decoder downstream is what has to catch it.
  //
  // Found by probing after the loop above reported "accepted 3-byte sequence: 0xED 0xA0 0x80 -> 0xD800".
  {
    const std::string surrogate = "\xED\xA0\x80";
    const char* cursor = surrogate.data();
    const char* const end = cursor + surrogate.size();

    EXPECT_EQ(Utf::nextUtf8(&cursor, end), 0xD800)
            << "the decoder returns a surrogate as a code point; if this becomes negative the defect is fixed";
    EXPECT_EQ(cursor, end) << "and it consumed the whole sequence, so this is acceptance rather than confusion";
  }

  // The neighbouring cases *are* refused, which is what makes the surrogate one a gap rather than a general laxity: an
  // overlong encoding of NUL is rejected, and so is a lead byte past the last valid one.
  {
    const std::string overlong = "\xC0\x80";
    const char* cursor = overlong.data();
    EXPECT_LT(Utf::nextUtf8(&cursor, overlong.data() + overlong.size()), 0) << "overlong encodings are refused";

    const std::string tooHigh = "\xF5\x80\x80\x80";
    cursor = tooHigh.data();
    EXPECT_LT(Utf::nextUtf8(&cursor, tooHigh.data() + tooHigh.size()), 0) << "0xF5 is not a valid lead byte";
  }
}

TEST(UtfTest, ReplacingDecodeSubstitutesAndThenStops) {
  // The lenient variant, whose contract is narrower than its name suggests. It is `nextUtf8` plus a fallback value, and
  // `nextUtf8` parks the cursor at the end when it fails -- so a malformed byte does not become one replacement
  // character and let the walk continue. It ends the walk, and the caller receives one U+FFFD for the whole remainder.
  //
  // That is the opposite of what this test first asserted, and the probe is what corrected it. A caller expecting the
  // forgiving behaviour gets a truncated decode preceded by a single replacement character.
  const std::string text = "a" "\xC3" "(z)";   // 'a', then an unterminated two-byte lead

  const char* cursor = text.data();
  const char* const end = cursor + text.size();

  EXPECT_EQ(Utf::nextUtf8WithReplacement(&cursor, end), 0x61) << "the first code point is unaffected";
  EXPECT_EQ(Utf::nextUtf8WithReplacement(&cursor, end), 0xFFFD) << "the malformed lead becomes a replacement";
  EXPECT_EQ(cursor, end) << "and the walk is over: nothing after the bad byte is decoded";
}

TEST(UtfTest, EncodesSurrogatePairsForTheSupplementaryPlanes) {
  // Anything above U+FFFF needs two UTF-16 units, and the arithmetic that splits it is easy to get wrong in a way that
  // produces a valid-looking pair for the wrong code point.
  struct Case { int32_t codePoint; uint16_t first; uint16_t second; };
  const Case cases[] = {
    { 0x10000, 0xD800, 0xDC00 },   // the lowest supplementary code point
    { 0x1F600, 0xD83D, 0xDE00 },   // an emoji
    { 0x10FFFF, 0xDBFF, 0xDFFF },  // the highest code point there is
  };

  for (const Case& testCase : cases) {
    uint16_t units[2] = {};
    const size_t length = Utf::toUtf16(testCase.codePoint, units);
    ASSERT_EQ(length, 2u) << "code point " << testCase.codePoint;
    EXPECT_EQ(units[0], testCase.first) << "high surrogate of " << testCase.codePoint;
    EXPECT_EQ(units[1], testCase.second) << "low surrogate of " << testCase.codePoint;

    // And the pair reads back as the original, which matters more than either half.
    const uint16_t* cursor = units;
    EXPECT_EQ(Utf::nextUtf16(&cursor, units + 2), testCase.codePoint);
  }

  // A basic-plane code point is one unit and is *not* a surrogate. The second slot must be left alone rather than
  // written with a zero, because a caller may be encoding into a buffer it is filling forward.
  uint16_t single[2] = { 0xFFFF, 0xFFFF };
  EXPECT_EQ(Utf::toUtf16(0x41, single), 1u);
  EXPECT_EQ(single[0], 0x41);
  EXPECT_EQ(single[1], 0xFFFF) << "a one-unit code point must not touch the second slot";

  // A null destination is the documented way to ask how many units a code point needs without encoding it.
  EXPECT_EQ(Utf::toUtf16(0x41, nullptr), 1u);
  EXPECT_EQ(Utf::toUtf16(0x1F600, nullptr), 2u);
  EXPECT_EQ(Utf::toUtf8(0x1F600, nullptr), 4u);
}

TEST(UtfTest, ConvertsAWholeBufferBetweenUtf8AndUtf16) {
  // `utf8ToUtf16` takes a byte count: that is what its parameter is called and what it does. It returns a *unit* count,
  // which is six rather than five because the emoji needs two units -- and that is what a caller sizing a destination
  // needs, so the two numbers are different on purpose.
  std::vector<uint16_t> utf16(kSampleUtf16Units + 1u, 0u);
  const int written = Utf::utf8ToUtf16(utf16.data(), utf16.size(), kSample, kSampleBytes);
  ASSERT_EQ(written, kSampleUtf16Units);
  EXPECT_NE(written, kSampleCodePoints) << "units and code points differ whenever text leaves the basic plane";

  const std::vector<uint16_t> expected { 0x61, 0xE9, 0x20AC, 0xD83D, 0xDE00, 0x7A };
  EXPECT_EQ(std::vector<uint16_t>(utf16.begin(), utf16.begin() + written), expected);

  // Asking for the length without a destination is the documented use of a null `dst`, and it has to agree with what the
  // real call wrote -- otherwise a caller sizing a buffer from the first call under-allocates on the second.
  EXPECT_EQ(Utf::utf8ToUtf16(nullptr, 0u, kSample, kSampleBytes), written);

  // And back, with the *unit* count that was just written. See the next test for why that is the right reading.
  std::vector<char> back(64, '\0');
  const int backWritten = Utf::utf16ToUtf8(back.data(), back.size(), utf16.data(), static_cast<size_t>(written));
  ASSERT_GT(backWritten, 0) << "the reverse conversion failed on text it had just produced";
  EXPECT_EQ(std::string(back.data(), static_cast<size_t>(backWritten)), std::string(kSample, kSampleBytes));
}

TEST(UtfTest, Utf16ToUtf8TreatsItsSizeAsUnitsNotBytes) {
  // The inconsistency, pinned. `utf8ToUtf16`'s last parameter is a byte count; `utf16ToUtf8`'s is a unit count, and the
  // header names both `srcByteSize`. A caller who believes the documentation and passes `units * sizeof(uint16_t)`
  // makes this function read twice as far as the buffer extends.
  //
  // The probe that found it: five units of data, a size argument of ten, and the walk continuing past the data into
  // padding until it met something that was not a surrogate pair -- fifteen characters from a ten-byte source.
  std::vector<uint16_t> utf16(kSampleUtf16Units + 8u, 0xFFFFu);   // 0xFFFF padding past the real data
  const int unitCount = Utf::utf8ToUtf16(utf16.data(), utf16.size(), kSample, kSampleBytes);
  ASSERT_EQ(unitCount, kSampleUtf16Units);

  std::vector<char> withUnits(64, '\0');
  const int asUnits = Utf::utf16ToUtf8(withUnits.data(), withUnits.size(), utf16.data(),
                                       static_cast<size_t>(unitCount));
  EXPECT_EQ(asUnits, static_cast<int>(kSampleBytes)) << "the unit count is the reading that stops in the right place";

  // The same buffer described in bytes, which is what the header invites. It reads past the data and produces more than
  // the source held. Stated as an assertion so that correcting the parameter's meaning changes this test rather than
  // silently invalidating it.
  std::vector<char> withBytes(64, '\0');
  const int asBytes = Utf::utf16ToUtf8(withBytes.data(), withBytes.size(), utf16.data(),
                                       static_cast<size_t>(unitCount) * sizeof(uint16_t));
  EXPECT_GT(asBytes, static_cast<int>(kSampleBytes))
          << "documented as a byte size and used as a unit count: passing bytes reads past the buffer";
}

TEST(UtfTest, Utf16ToUtf8DoesNotReportAnOverflowingDestination) {
  // The second defect. The header says of both conversions: "If there is an error, -1 is returned and the dst[] buffer
  // is undefined." In fact neither refuses a destination too small for the result -- this one writes what fits, abandons
  // a code point mid-sequence, and returns the length it *would* have written.
  //
  // So a caller who sizes a buffer from the return value receives a successfully-encoded prefix and is told nothing. The
  // probe: capacity one, four bytes reported, one byte stored, the remainder of the buffer untouched -- a truncated
  // UTF-8 sequence presented as a success.
  const uint16_t pair[2] = { 0xD83D, 0xDE00 };       // one four-byte code point

  char oneByte[1] = { 'X' };
  const int reported = Utf::utf16ToUtf8(oneByte, 1u, pair, 2u);

  EXPECT_EQ(reported, 4) << "the return counts what would have been written, not what fitted";
  EXPECT_EQ(oneByte[0], static_cast<char>(0xF0)) << "and only the first byte of the sequence was stored";
  EXPECT_GT(reported, 0) << "if this ever becomes -1 the function has been changed to refuse overflow, which is what "
                            "its documentation already claims it does";
}

TEST(UtfTest, NextUtf32AcceptsASurrogateAndAValuePastTheLastCodePoint) {
  // The third defect, and the only one reachable without a caller opting in: UTF-32 is nominally one code point per
  // unit, so `nextUtf32`'s job is to hand units back with the validity check the other two decoders perform. It performs
  // none -- a lone surrogate, and a value beyond U+10FFFF, both come back as themselves.
  //
  // U+D800 is explicitly not a Unicode scalar value. Anything downstream that stores the result as a code point, or
  // re-encodes it, produces invalid UTF-8 as a consequence.
  const int32_t units[] = { 0x61, 0xD800, 0x110000, 0x1F600 };
  const int32_t* cursor = units;
  const int32_t* const end = units + 4;

  EXPECT_EQ(Utf::nextUtf32(&cursor, end), 0x61);
  EXPECT_EQ(Utf::nextUtf32(&cursor, end), 0xD800) << "a surrogate is returned as-is";
  EXPECT_EQ(Utf::nextUtf32(&cursor, end), 0x110000) << "and so is a value past the last code point";
  EXPECT_EQ(Utf::nextUtf32(&cursor, end), 0x1F600);
  EXPECT_EQ(cursor, end);

  // For contrast, the *encoders* refuse a value past the last code point but not a surrogate. So the module holds four
  // different answers to "is this a code point", which is the real finding: not that one of them is wrong, but that they
  // disagree, so a round trip through the module is not the identity for every input.
  EXPECT_EQ(Utf::toUtf8(0x110000, nullptr), 0u) << "the encoder refuses what the UTF-32 decoder accepted";
  EXPECT_GT(Utf::toUtf8(0xD800, nullptr), 0u) << "and the encoder accepts a surrogate the standard does not";
}

} // namespace anonymous
} // namespace utils
