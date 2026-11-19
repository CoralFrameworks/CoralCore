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

#ifndef CORAL_UTILS_COMPILER_H
#define CORAL_UTILS_COMPILER_H

#if !defined(CORAL_BUILD_FOR_ANDROID) && !defined(CORAL_BUILD_FOR_IOS) && \
    !defined(CORAL_BUILD_FOR_WIN) && !defined(CORAL_BUILD_FOR_LINUX) && \
    !defined(CORAL_BUILD_FOR_MAC)

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(_WIN32) || defined(__SYMBIAN32__)
#define CORAL_BUILD_FOR_WIN
#elif defined(ANDROID) || defined(__ANDROID__)
#define CORAL_BUILD_FOR_ANDROID
#elif defined(linux) || defined(__linux) || defined(__FreeBSD__) || \
      defined(__OpenBSD__) || defined(__sun) || defined(__NetBSD__) || \
      defined(__DragonFly__) || defined(__Fuchsia__) || \
      defined(__GLIBC__) || defined(__GNU__) || defined(__unix__)
#define CORAL_BUILD_FOR_LINUX
#elif TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#define CORAL_BUILD_FOR_IOS
#else
#define CORAL_BUILD_FOR_MAC
#endif
#endif // end CORAL_BUILD_FOR_*

#if !defined(CORAL_CPU_BENDIAN) && !defined(CORAL_CPU_LENDIAN)
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define CORAL_CPU_BENDIAN
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define CORAL_CPU_LENDIAN
#elif defined(__sparc) || defined(__sparc__) || \
      defined(_POWER) || defined(__powerpc__) || \
      defined(__ppc__) || defined(__hppa) || \
      defined(__PPC__) || defined(__PPC64__) || \
      defined(_MIPSEB) || defined(__ARMEB__) || \
      defined(__s390__) || \
      (defined(__sh__) && defined(__BIG_ENDIAN__)) || \
      (defined(__ia64) && defined(__BIG_ENDIAN__))
#define CORAL_CPU_BENDIAN
#else
#define CORAL_CPU_LENDIAN
#endif
#endif

#if defined(__i386) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
#define CORAL_CPU_X86 1
#endif

#if defined(__loongarch__) || defined (__loongarch64)
#define CORAL_CPU_LOONGARCH 1
#endif

#if defined(__powerpc__) || defined (__powerpc64__)
#define CORAL_CPU_PPC 1
#endif

/**
 *  CORAL_CPU_SSE_LEVEL
 *
 *  If defined, CORAL_CPU_SSE_LEVEL should be set to the highest supported level.
 *  On non-intel CPU this should be undefined.
 */
#define CORAL_CPU_SSE_LEVEL_SSE1     10
#define CORAL_CPU_SSE_LEVEL_SSE2     20
#define CORAL_CPU_SSE_LEVEL_SSE3     30
#define CORAL_CPU_SSE_LEVEL_SSSE3    31
#define CORAL_CPU_SSE_LEVEL_SSE41    41
#define CORAL_CPU_SSE_LEVEL_SSE42    42
#define CORAL_CPU_SSE_LEVEL_AVX      51
#define CORAL_CPU_SSE_LEVEL_AVX2     52
#define CORAL_CPU_SSE_LEVEL_SKX      60

/**
 *  CORAL_CPU_LSX_LEVEL
 *
 *  If defined, CORAL_CPU_LSX_LEVEL should be set to the highest supported level.
 *  On non-loongarch CPU this should be undefined.
 */
#define CORAL_CPU_LSX_LEVEL_LSX      70
#define CORAL_CPU_LSX_LEVEL_LASX     80

// Are we in GCC/Clang?
#ifndef CORAL_CPU_SSE_LEVEL
// These checks must be done in descending order to ensure we set the highest
// available SSE level.
#if defined(__AVX512F__) && defined(__AVX512DQ__) && defined(__AVX512CD__) && \
        defined(__AVX512BW__) && defined(__AVX512VL__)
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_SKX
#elif defined(__AVX2__)
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_AVX2
#elif defined(__AVX__)
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_AVX
#elif defined(__SSE4_2__)
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_SSE42
#elif defined(__SSE4_1__)
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_SSE41
#elif defined(__SSSE3__)
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_SSSE3
#elif defined(__SSE3__)
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_SSE3
#elif defined(__SSE2__)
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_SSE2
#endif
#endif

#ifndef CORAL_CPU_LSX_LEVEL
#if defined(__loongarch_asx)
#define CORAL_CPU_LSX_LEVEL    CORAL_CPU_LSX_LEVEL_LASX
#elif defined(__loongarch_sx)
#define CORAL_CPU_LSX_LEVEL    CORAL_CPU_LSX_LEVEL_LSX
#endif
#endif

// Are we in Visual Studio?
#ifndef CORAL_CPU_SSE_LEVEL
// These checks must be done in descending order to ensure we set the highest
// available SSE level. 64-bit intel guarantees at least SSE2 support.
#if defined(__AVX512F__) && defined(__AVX512DQ__) && defined(__AVX512CD__) && \
        defined(__AVX512BW__) && defined(__AVX512VL__)
#define CORAL_CPU_SSE_LEVEL        CORAL_CPU_SSE_LEVEL_SKX
#elif defined(__AVX2__)
#define CORAL_CPU_SSE_LEVEL        CORAL_CPU_SSE_LEVEL_AVX2
#elif defined(__AVX__)
#define CORAL_CPU_SSE_LEVEL        CORAL_CPU_SSE_LEVEL_AVX
#elif defined(_M_X64) || defined(_M_AMD64)
#define CORAL_CPU_SSE_LEVEL        CORAL_CPU_SSE_LEVEL_SSE2
#elif defined(_M_IX86_FP)
#if _M_IX86_FP >= 2
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_SSE2
#elif _M_IX86_FP == 1
#define CORAL_CPU_SSE_LEVEL    CORAL_CPU_SSE_LEVEL_SSE1
#endif
#endif
#endif

// ARM defines
#if defined(__arm__) && (!defined(__APPLE__) || !TARGET_IPHONE_SIMULATOR)
#define CORAL_CPU_ARM32
#elif defined(__aarch64__)
#define CORAL_CPU_ARM64
#endif

// All 64-bit ARM chips have NEON.  Many 32-bit ARM chips do too.
#if !defined(CORAL_ARM_HAS_NEON) && defined(__ARM_NEON)
#define CORAL_ARM_HAS_NEON
#endif

// compatibility with non-clang compilers...
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if __has_attribute(visibility)
#define CORAL_PUBLIC  __attribute__((visibility("default")))
#else
#define CORAL_PUBLIC
#endif

#if __has_attribute(deprecated)
#define CORAL_DEPRECATED [[deprecated]]
#else
#define CORAL_DEPRECATED
#endif

#if __has_attribute(packed)
#define CORAL_PACKED __attribute__((packed))
#else
#define CORAL_PACKED
#endif

#if __has_attribute(noreturn)
#define CORAL_NORETURN __attribute__((noreturn))
#else
#define CORAL_NORETURN
#endif

#if __has_attribute(fallthrough)
#define CORAL_FALLTHROUGH [[fallthrough]]
#else
#define CORAL_FALLTHROUGH
#endif

#if __has_attribute(visibility)
#ifndef CORAL_DEV
#define CORAL_PRIVATE __attribute__((visibility("hidden")))
#else
#define CORAL_PRIVATE
#endif
#else
#define CORAL_PRIVATE
#endif

#define CORAL_NO_SANITIZE_THREAD
#if __has_feature(thread_sanitizer)
#undef CORAL_NO_SANITIZE_THREAD
#define CORAL_NO_SANITIZE_THREAD __attribute__((no_sanitize("thread")))
#endif

#define CORAL_HAS_SANITIZE_THREAD 0
#if __has_feature(thread_sanitizer) || defined(__SANITIZE_THREAD__)
#undef CORAL_HAS_SANITIZE_THREAD
#define CORAL_HAS_SANITIZE_THREAD 1
#endif

#define CORAL_HAS_SANITIZE_MEMORY 0
#if __has_feature(memory_sanitizer)
#undef CORAL_HAS_SANITIZE_MEMORY
#define CORAL_HAS_SANITIZE_MEMORY 1
#endif

/*
 * helps the compiler's optimizer predicting branches
 */
#if __has_builtin(__builtin_expect)
#ifdef __cplusplus
#define CORAL_LIKELY( exp )    (__builtin_expect( !!(exp), true ))
#define CORAL_UNLIKELY( exp )  (__builtin_expect( !!(exp), false ))
#else
#define CORAL_LIKELY( exp )    (__builtin_expect( !!(exp), 1 ))
#define CORAL_UNLIKELY( exp )  (__builtin_expect( !!(exp), 0 ))
#endif
#else
#define CORAL_LIKELY( exp )    (!!(exp))
#define CORAL_UNLIKELY( exp )  (!!(exp))
#endif

#if __has_builtin(__builtin_expect_with_probability)
#ifdef __cplusplus
#define CORAL_VERY_LIKELY( exp )    (__builtin_expect_with_probability( !!(exp), true, 0.995 ))
#define CORAL_VERY_UNLIKELY( exp )  (__builtin_expect_with_probability( !!(exp), false, 0.995 ))
#else
#define CORAL_VERY_LIKELY( exp )    (__builtin_expect_with_probability( !!(exp), 1, 0.995 ))
#define CORAL_VERY_UNLIKELY( exp )  (__builtin_expect_with_probability( !!(exp), 0, 0.995 ))
#endif
#else
#define CORAL_VERY_LIKELY( exp )    (!!(exp))
#define CORAL_VERY_UNLIKELY( exp )  (!!(exp))
#endif

#if __has_builtin(__builtin_prefetch)
#define CORAL_PREFETCH( exp ) (__builtin_prefetch(exp))
#else
#define CORAL_PREFETCH( exp )
#endif

#if __has_builtin(__builtin_assume)
#define CORAL_ASSUME( exp ) (__builtin_assume(exp))
#else
#define CORAL_ASSUME( exp )
#endif

#if (defined(__i386__) || defined(__x86_64__))
#define CORAL_HAS_HYPER_THREADING 1      // on x86 we assume we have hyper-threading.
#else
#define CORAL_HAS_HYPER_THREADING 0
#endif

#if defined(CORAL_SINGLE_THREADED)
#define CORAL_HAS_THREADING 0
#elif defined(__EMSCRIPTEN__)
#if defined(__EMSCRIPTEN_PTHREADS__) && defined(CORAL_WASM_THREADS)
#define CORAL_HAS_THREADING 1
#else
#define CORAL_HAS_THREADING 0
#endif
#else
#define CORAL_HAS_THREADING 1
#endif

#if __has_attribute(noinline)
#define CORAL_NOINLINE __attribute__((noinline))
#else
#define CORAL_NOINLINE
#endif

#if __has_attribute(always_inline)
#define CORAL_ALWAYS_INLINE __attribute__((always_inline))
#else
#define CORAL_ALWAYS_INLINE
#endif

#if __has_attribute(pure)
#define CORAL_PURE __attribute__((pure))
#else
#define CORAL_PURE
#endif

#if __has_attribute(maybe_unused) || (defined(_MSC_VER) && _MSC_VER >= 1911)
#define CORAL_UNUSED [[maybe_unused]]
#define CORAL_UNUSED_IN_RELEASE [[maybe_unused]]
#elif __has_attribute(unused)
#define CORAL_UNUSED __attribute__((unused))
#define CORAL_UNUSED_IN_RELEASE __attribute__((unused))
#else
#define CORAL_UNUSED
#define CORAL_UNUSED_IN_RELEASE
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1900
#define CORAL_RESTRICT __restrict
#elif (defined(__clang__) || defined(__GNUC__))
#define CORAL_RESTRICT __restrict__
#else
#define CORAL_RESTRICT
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1900
#define CORAL_HAS_FEATURE_CXX_THREAD_LOCAL 1
#elif __has_feature(cxx_thread_local)
#define CORAL_HAS_FEATURE_CXX_THREAD_LOCAL 1
#else
#define CORAL_HAS_FEATURE_CXX_THREAD_LOCAL 0
#endif

#if defined(__clang__)
#define CORAL_NONNULL _Nonnull
#define CORAL_NULLABLE _Nullable
#else
#define CORAL_NONNULL
#define CORAL_NULLABLE
#endif

#if defined(_MSC_VER)
// MSVC does not support loop unrolling hints
#define CORAL_UNROLL
#define CORAL_NOUNROLL
#else
// C++11 allows pragmas to be specified as part of defines using the _Pragma syntax.
#define CORAL_UNROLL _Pragma("unroll")
#define CORAL_NOUNROLL _Pragma("nounroll")
#endif

#if __has_feature(cxx_rtti) || defined(_CPPRTTI)
#define CORAL_HAS_RTTI 1
#else
#define CORAL_HAS_RTTI 0
#endif

#ifdef __ARM_ACLE
#include <arm_acle.h>
#define CORAL_WAIT_FOR_INTERRUPT()   __wfi()
#define CORAL_WAIT_FOR_EVENT()       __wfe()
#define CORAL_BROADCAST_EVENT()      __sev()
#define CORAL_SIGNAL_EVENT()         __sevl()
#define CORAL_PAUSE()                __yield()
#define CORAL_PREFETCHW(addr)        __pldx(1, 0, 0, addr)
#else // !__ARM_ACLE
#if (defined(__i386__) || defined(__x86_64__))
#define CORAL_X86_PAUSE              {__asm__ __volatile__( "rep; nop" : : : "memory" );}
#define CORAL_WAIT_FOR_INTERRUPT()   CORAL_X86_PAUSE
#define CORAL_WAIT_FOR_EVENT()       CORAL_X86_PAUSE
#define CORAL_BROADCAST_EVENT()
#define CORAL_SIGNAL_EVENT()
#define CORAL_PAUSE()                CORAL_X86_PAUSE
#define CORAL_PREFETCHW(addr)        CORAL_PREFETCH(addr)
#else // !x86
#define CORAL_WAIT_FOR_INTERRUPT()
#define CORAL_WAIT_FOR_EVENT()
#define CORAL_BROADCAST_EVENT()
#define CORAL_SIGNAL_EVENT()
#define CORAL_PAUSE()
#define CORAL_PREFETCHW(addr)        CORAL_PREFETCH(addr)
#endif // x86
#endif // __ARM_ACLE

// ssize_t is a POSIX type.
#if defined(WIN32) || defined(_WIN32)
#include <Basetsd.h>
typedef SSIZE_T ssize_t;
#endif

#ifdef _MSC_VER
#define CORAL_EMPTY_BASES __declspec(empty_bases)
#else
#define CORAL_EMPTY_BASES
#endif

#if defined(WIN32) || defined(_WIN32)
#define IMPORTSYMB __declspec(dllimport)
#else
#define IMPORTSYMB
#endif

#if defined(_MSC_VER) && !defined(__PRETTY_FUNCTION__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#if defined(_MSC_VER)
#define CORAL_WARNING_PUSH _Pragma("warning( push )")
#define CORAL_WARNING_POP _Pragma("warning( pop )")
#define CORAL_WARNING_ENABLE_PADDED _Pragma("warning(1: 4324)")
#elif defined(__clang__)
#define CORAL_WARNING_PUSH _Pragma("clang diagnostic push")
#define CORAL_WARNING_POP  _Pragma("clang diagnostic pop")
#define CORAL_WARNING_ENABLE_PADDED _Pragma("clang diagnostic warning \"-Wpadded\"")
#else
#define CORAL_WARNING_PUSH
#define CORAL_WARNING_POP
#define CORAL_WARNING_ENABLE_PADDED
#endif

#endif //CORAL_UTILS_COMPILER_H
