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

#ifndef CORAL_UTILS_ASSERT_H
#define CORAL_UTILS_ASSERT_H

#include <cassert>
#include <cstdint>

namespace utils {

void panic(const char* func, const char* file, int line, const char* assertion, const char* msg);

} // namespace utils

#if __has_builtin(__builtin_expect)
#ifdef __cplusplus
#define ASSERT_LIKELY( exp )    (__builtin_expect( !!(exp), true ))
#else
#define ASSERT_LIKELY( exp )    (__builtin_expect( !!(exp), 1 ))
#endif
#else
#define ASSERT_LIKELY( exp )    (!!(exp))
#endif


#ifdef NDEBUG
#define CORAL_IMPL_ASSERT(e)        ((void)0)
#define CORAL_IMPL_ASSERTX(e, msg) ((void)0)
#else
#define CORAL_IMPL_ASSERT(e) \
            (ASSERT_LIKELY(e) ? ((void)0) : utils::panic(__func__, __FILE__, __LINE__, #e, ""))

#define CORAL_IMPL_ASSERTX(e, msg) \
            (ASSERT_LIKELY(e) ? ((void)0) : utils::panic(__func__, __FILE__, __LINE__, #e, msg))
#endif

/**
 * Asserts that the passed condition evaluates to true.
 *
 * @param condition the expression
 */
#define CORAL_ASSERT(condition) CORAL_IMPL_ASSERT(condition)

/**
 * Asserts that the passed condition evaluates to true. Adds extra message string.
 *
 * @param condition the expression
 * @param msg string literal which will be appended to the error message
 */
#define CORAL_ASSERTX(condition, msg) CORAL_IMPL_ASSERTX(condition, msg)

#if !defined(CrUNREACHABLE)
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#define FAST_FAIL_INVALID_ARG  5
// See https://developercommunity.visualstudio.com/content/problem/1128631/code-flow-doesnt-see-noreturn-with-extern-c.html
// for why this is wrapped. Hopefully removable after msvc++ 19.27 is no longer supported.
[[noreturn]] static void utils_fast_fail() { __fastfail(FAST_FAIL_INVALID_ARG); }
#define CrUNREACHABLE utils_fast_fail()
#else
#define CrUNREACHABLE __builtin_trap()
#endif
#endif

#endif //CORAL_UTILS_ASSERT_H
