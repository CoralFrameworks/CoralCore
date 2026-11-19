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

#ifndef CORAL_UTILS_SSTREAM_H
#define CORAL_UTILS_SSTREAM_H

#include "private/utils/ostream.h"

namespace utils::io {

class sstream : public ostream {
public:
  ostream& flush() noexcept override;

  [[nodiscard]] const char* c_str() const noexcept;

  [[nodiscard]] size_t length() const noexcept;
};

} // namespace utils::io

#endif //CORAL_UTILS_SSTREAM_H
