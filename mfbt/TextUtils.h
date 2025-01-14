/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Character/text operations. */

#ifndef mozilla_TextUtils_h
#define mozilla_TextUtils_h

#include "mozilla/TypeTraits.h"

namespace mozilla {

namespace detail {

template<typename Char>
class MakeUnsignedChar
  : public MakeUnsigned<Char>
{};

template<>
class MakeUnsignedChar<char16_t>
{
public:
  typedef char16_t Type;
};

template<>
class MakeUnsignedChar<char32_t>
{
public:
  typedef char32_t Type;
};

} // namespace detail

/**
 * Returns true if |aChar| matches [a-zA-Z].
 *
 * This function is basically what you thought isalpha was, except its behavior
 * doesn't depend on the user's current locale.
 */
template<typename Char>
const bool 
IsAsciiAlpha(Char aChar)
{
  typedef typename detail::MakeUnsignedChar<Char>::Type UnsignedChar;
  return ('a' <= static_cast<UnsignedChar>(aChar) &&
          static_cast<UnsignedChar>(aChar) <= 'z') ||
         ('A' <= static_cast<UnsignedChar>(aChar) &&
          static_cast<UnsignedChar>(aChar) <= 'Z');
}

} // namespace mozilla

#endif /* mozilla_TextUtils_h */
