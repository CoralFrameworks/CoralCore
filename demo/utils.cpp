/*
 * Copyright (C) 2026 The Coral Project
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

// =============================================================
//  utils.cpp
//
//  The small value helpers in `utils`: Base64, Utf and Path. They
//  have no threading or platform loop behind them, so the demo
//  links only the `utils` library.
//
//    coralcore_demo_utils
// =============================================================

#include <utils/Base64.h>
#include <utils/Path.h>
#include <utils/Utf.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int gChecks = 0;
int gFailures = 0;

void check(const bool condition, const char* what) {
  ++gChecks;
  std::printf("  %s %s\n", condition ? "ok  " : "FAIL", what);
  if (!condition) {
    ++gFailures;
  }
}

} // namespace

int main() {
  std::printf("CoralCore demo: utils\n\n");

  // ------------------------------------------------------------------------------------------------
  // Base64: encode into a buffer sized by encodedSize(), then decode back and compare.
  // ------------------------------------------------------------------------------------------------
  const std::string source = "CoralCore framework";
  const size_t encodedSize = utils::Base64::encodedSize(source.size());
  std::string encoded(encodedSize, '\0');
  const size_t written = utils::Base64::encode(source.data(), source.size(), encoded.data());

  std::printf("  \"%s\" -> \"%s\"\n", source.c_str(), encoded.c_str());
  check(written == encodedSize, "Base64::encode() returns the encoded size");

  std::string decoded(source.size(), '\0');
  size_t decodedSize = 0;
  const utils::Base64::Error error =
          utils::Base64::decode(encoded.c_str(), encoded.size(), decoded.data(), &decodedSize);

  check(error == utils::Base64::kNoError, "Base64::decode() reports no error");
  check(decodedSize == source.size() && decoded == source, "the Base64 round trip preserved every byte");

  // ------------------------------------------------------------------------------------------------
  // Utf: the sample is "a", e-acute, the euro sign, a grinning emoji and "z" -- five code points in
  // eleven UTF-8 bytes, because the bytes are one, two, three and four wide. The adjacent string
  // literal breaks are deliberate: a \x escape consumes every hex digit that follows it.
  // ------------------------------------------------------------------------------------------------
  constexpr char kSample[] = "a\xC3\xA9"
                             "\xE2\x82\xAC"
                             "\xF0\x9F\x98\x80"
                             "z";
  constexpr size_t kSampleBytes = 11;
  constexpr int kSampleCodePoints = 5;

  check(utils::Utf::getCount(kSample, kSampleBytes) == kSampleCodePoints,
          "Utf::getCount() counts code points, not bytes");

  std::vector<int32_t> codePoints;
  const char* cursor = kSample;
  const char* const end = kSample + kSampleBytes;
  while (cursor < end) {
    codePoints.push_back(utils::Utf::nextUtf8(&cursor, end));
  }

  check(codePoints.size() == kSampleCodePoints && cursor == end, "Utf::nextUtf8() consumed the whole buffer");
  check(codePoints.size() == kSampleCodePoints && codePoints[3] == 0x1F600,
          "the four-byte emoji came back as one code point");

  // Converting to UTF-16 is how a Windows API call takes the same text.
  std::vector<uint16_t> utf16(utils::Utf::getCount(kSample, kSampleBytes) * 2);
  const int utf16Units = utils::Utf::utf8ToUtf16(utf16.data(), utf16.size(), kSample, kSampleBytes);
  check(utf16Units == 6, "utf8ToUtf16() returns six UTF-16 units for five code points");

  // ------------------------------------------------------------------------------------------------
  // Path: string manipulation only; the filesystem is touched only by getCurrentDirectory().
  // ------------------------------------------------------------------------------------------------
  const utils::Path scene("assets/scene.json");
  check(scene.getName() == "scene.json", "Path::getName() returns the leaf");
  check(scene.getExtension() == "json", "Path::getExtension() returns the suffix");
  check(scene.getParent().getName() == "assets", "Path::getParent() peels the leaf");
  check(utils::Path::concat("assets", "scene.json").getName() == "scene.json",
          "Path::concat() joins a root and a leaf");

  const utils::Path currentDirectory = utils::Path::getCurrentDirectory();
  std::printf("  current directory: %s\n", currentDirectory.c_str());
  check(!currentDirectory.isEmpty() && currentDirectory.isAbsolute(),
          "Path::getCurrentDirectory() returns an absolute path");

  std::printf("\n%d checks, %d failures\n", gChecks, gFailures);
  std::printf("%s\n", gFailures == 0 ? "coralcore_demo_utils: PASS" : "coralcore_demo_utils: FAIL");
  return gFailures == 0 ? 0 : 1;
}
