// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Helper function for bit twiddling and macros for branch prediction.

#ifndef WOFF2_PORT_H_
#define WOFF2_PORT_H_

#include <assert.h>
#if defined(_MSC_VER) && _MSC_VER >= 1800
#include <inttypes.h>
#else
#include "mozilla/MSIntTypes.h"
#endif

namespace woff2 {

typedef unsigned int       uint32;

inline int Log2Floor(uint32 n) {
#if defined(__GNUC__)
  return n == 0 ? -1 : 31 ^ __builtin_clz(n);
#else
  if (n == 0)
    return -1;
  int log = 0;
  uint32 value = n;
  for (int i = 4; i >= 0; --i) {
    int shift = (1 << i);
    uint32 x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  assert(value == 1);
  return log;
#endif
}

} // namespace woff2

/* Compatibility with non-clang compilers. */
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ > 95) || \
    (defined(__llvm__) && __has_builtin(__builtin_expect))
#define PREDICT_FALSE(x) (__builtin_expect(x, 0))
#define PREDICT_TRUE(x) (__builtin_expect(!!(x), 1))
#else
#define PREDICT_FALSE(x) (x)
#define PREDICT_TRUE(x) (x)
#endif

#if (defined(__ARM_ARCH) && (__ARM_ARCH == 7)) || \
    (defined(M_ARM) && (M_ARM == 7)) || \
    defined(__aarch64__) || defined(__ARM64_ARCH_8__) || defined(__i386) || \
    defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define WOFF_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define WOFF_BIG_ENDIAN
#endif  /* endianness */
#endif  /* CPU whitelist */

#endif  // WOFF2_PORT_H_
