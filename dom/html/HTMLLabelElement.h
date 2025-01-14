/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Declaration of HTML <label> elements.
 */
#ifndef HTMLLabelElement_h
#define HTMLLabelElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLLabelElement.h"

namespace mozilla {
class EventChainPostVisitor;
namespace dom {

class HTMLLabelElement final : public nsGenericHTMLFormElement,
                                   public nsIDOMHTMLLabelElement
{
public:
  explicit HTMLLabelElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLFormElement(aNodeInfo),
      mHandlingEvent(false)
  {
  }

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLLabelElement, label)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override
  {
    return true;
  }

  // nsIDOMHTMLLabelElement
  NS_DECL_NSIDOMHTMLLABELELEMENT

  using nsGenericHTMLFormElement::GetForm;
  void GetHtmlFor(nsString& aHtmlFor)
  {
    GetHTMLAttr(nsGkAtoms::_for, aHtmlFor);
  }
  void SetHtmlFor(const nsAString& aHtmlFor, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::_for, aHtmlFor, aError);
  }
  nsGenericHTMLElement* GetControl() const
  {
    return GetLabeledElement();
  }

  using nsGenericHTMLElement::Focus;
  virtual void Focus(mozilla::ErrorResult& aError) override;

  // nsIFormControl
  NS_IMETHOD_(uint32_t) GetType() const override { return NS_FORM_LABEL; }
  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission) override;

  // nsIContent
  virtual nsresult PostHandleEvent(
                     EventChainPostVisitor& aVisitor) override;
  virtual void PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent) override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  nsGenericHTMLElement* GetLabeledElement() const;
protected:
  virtual ~HTMLLabelElement();

  virtual JSObject* WrapNode(JSContext *aCx) override;

  nsGenericHTMLElement* GetFirstLabelableDescendant() const;

  // XXX It would be nice if we could use an event flag instead.
  bool mHandlingEvent;
};

} // namespace dom
} // namespace mozilla

#endif /* HTMLLabelElement_h */
