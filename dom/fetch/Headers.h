/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Headers_h
#define mozilla_dom_Headers_h

#include "mozilla/dom/HeadersBinding.h"

#include "nsClassHashtable.h"
#include "nsWrapperCache.h"

#include "InternalHeaders.h"

class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

template<typename T> class MozMap;
class HeadersOrByteStringSequenceSequenceOrByteStringMozMap;
class OwningHeadersOrByteStringSequenceSequenceOrByteStringMozMap;

/**
 * This Headers class is only used to represent the content facing Headers
 * object. It is actually backed by an InternalHeaders implementation. Goanna
 * code should NEVER use this, except in the Request and Response
 * implementations, where they must always be created from the backing
 * InternalHeaders object.
 */
class Headers final : public nsISupports
                        , public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Headers)

  friend class Request;
  friend class Response;

private:
  nsCOMPtr<nsISupports> mOwner;
  nsRefPtr<InternalHeaders> mInternalHeaders;

public:
  explicit Headers(nsISupports* aOwner, InternalHeaders* aInternalHeaders)
    : mOwner(aOwner)
    , mInternalHeaders(aInternalHeaders)
  {
  }

  explicit Headers(const Headers& aOther) MOZ_DELETE;

  static bool PrefEnabled(JSContext* cx, JSObject* obj);

  static already_AddRefed<Headers>
  Constructor(const GlobalObject& aGlobal,
              const Optional<HeadersOrByteStringSequenceSequenceOrByteStringMozMap>& aInit,
              ErrorResult& aRv);

  static already_AddRefed<Headers>
  Constructor(const GlobalObject& aGlobal,
              const OwningHeadersOrByteStringSequenceSequenceOrByteStringMozMap& aInit,
              ErrorResult& aRv);

  void Append(const nsACString& aName, const nsACString& aValue,
              ErrorResult& aRv)
  {
    mInternalHeaders->Append(aName, aValue, aRv);
  }

  void Delete(const nsACString& aName, ErrorResult& aRv)
  {
    mInternalHeaders->Delete(aName, aRv);
  }

  void Get(const nsACString& aName, nsCString& aValue, ErrorResult& aRv) const
  {
    mInternalHeaders->Get(aName, aValue, aRv);
  }

  void GetAll(const nsACString& aName, nsTArray<nsCString>& aResults,
              ErrorResult& aRv) const
  {
    mInternalHeaders->GetAll(aName, aResults, aRv);
  }

  bool Has(const nsACString& aName, ErrorResult& aRv) const
  {
    return mInternalHeaders->Has(aName, aRv);
  }

  void Set(const nsACString& aName, const nsACString& aValue, ErrorResult& aRv)
  {
    mInternalHeaders->Set(aName, aValue, aRv);
  }

  // ChromeOnly
  HeadersGuardEnum Guard() const
  {
    return mInternalHeaders->Guard();
  }

  void SetGuard(HeadersGuardEnum aGuard, ErrorResult& aRv)
  {
    mInternalHeaders->SetGuard(aGuard, aRv);
  }

  virtual JSObject* WrapObject(JSContext* aCx) override;
  nsISupports* GetParentObject() const { return mOwner; }

private:
  virtual ~Headers();

  InternalHeaders*
  GetInternalHeaders() const
  {
    return mInternalHeaders;
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Headers_h
