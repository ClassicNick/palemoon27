/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGPolylineElement_h
#define mozilla_dom_SVGPolylineElement_h

#include "nsSVGPolyElement.h"

nsresult NS_NewSVGPolylineElement(nsIContent **aResult,
                                  already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

typedef nsSVGPolyElement SVGPolylineElementBase;

namespace mozilla {
namespace dom {

class SVGPolylineElement final : public SVGPolylineElementBase
{
protected:
  explicit SVGPolylineElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx) override;
  friend nsresult (::NS_NewSVGPolylineElement(nsIContent **aResult,
                                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

  // nsSVGPathGeometryElement methods:
  virtual already_AddRefed<Path> BuildPath(PathBuilder* aBuilder) override;

public:
  // nsIContent interface
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;
};

} // namespace mozilla
} // namespace dom

#endif // mozilla_dom_SVGPolylineElement_h
