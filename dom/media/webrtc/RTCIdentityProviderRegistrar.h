/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RTCIDENTITYPROVIDER_H_
#define RTCIDENTITYPROVIDER_H_

#include "mozilla/nsRefPtr.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsIGlobalObject.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/BindingDeclarations.h"

namespace mozilla {
namespace dom {

class RTCIdentityProvider;

class RTCIdentityProviderRegistrar final : public nsISupports,
                                               public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(RTCIdentityProviderRegistrar)

  explicit RTCIdentityProviderRegistrar(nsIGlobalObject* aGlobal);

  // As required
  nsIGlobalObject* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx);

  // setter and getter
  void Register(RTCIdentityProvider& aIdp);
  already_AddRefed<RTCIdentityProvider> GetIdp();

  already_AddRefed<Promise>
  GenerateAssertion(const nsAString& aContents, const nsAString& aOrigin,
                    const Optional<nsAString>& aUsernameHint, ErrorResult& aRv);
  already_AddRefed<Promise>
  ValidateAssertion(const nsAString& assertion, const nsAString& origin,
                    ErrorResult& aRv);

private:
  ~RTCIdentityProviderRegistrar();

  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsRefPtr<RTCIdentityProvider> mIdp;
};

} // namespace dom
} // namespace mozilla

#endif /* RTCIDENTITYPROVIDER_H_ */
