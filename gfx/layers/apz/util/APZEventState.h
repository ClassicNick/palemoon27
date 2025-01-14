/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZEventState_h
#define mozilla_layers_APZEventState_h

#include <stdint.h>

#include "FrameMetrics.h"     // for ScrollableLayerGuid
#include "Units.h"
#include "mozilla/EventForwards.h"
#include "mozilla/layers/GoannaContentController.h"  // for APZStateChange
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"  // for NS_INLINE_DECL_REFCOUNTING
#include "nsIWeakReferenceUtils.h"  // for nsWeakPtr
#include "mozilla/nsRefPtr.h"
#include "nsIDocument.h"

template <class> class nsCOMPtr;
class nsIDOMWindowUtils;
class nsIWidget;

namespace mozilla {
namespace layers {

class ActiveElementManager;

struct ContentReceivedInputBlockCallback {
public:
  NS_INLINE_DECL_REFCOUNTING(ContentReceivedInputBlockCallback);
  virtual void Run(const ScrollableLayerGuid& aGuid,
                   uint64_t aInputBlockId,
                   bool aPreventDefault) const = 0;
protected:
  virtual ~ContentReceivedInputBlockCallback() {}
};

/**
 * A content-side component that keeps track of state for handling APZ
 * gestures and sending APZ notifications.
 */
class APZEventState {
  typedef GoannaContentController::APZStateChange APZStateChange;
  typedef FrameMetrics::ViewID ViewID;
public:
  APZEventState(nsIWidget* aWidget,
                const nsRefPtr<ContentReceivedInputBlockCallback>& aCallback);

  NS_INLINE_DECL_REFCOUNTING(APZEventState);

  void ProcessSingleTap(const CSSPoint& aPoint,
                        const ScrollableLayerGuid& aGuid,
                        float aPresShellResolution);
  void ProcessLongTap(const nsCOMPtr<nsIDOMWindowUtils>& aUtils,
                      const CSSPoint& aPoint,
                      const ScrollableLayerGuid& aGuid,
                      uint64_t aInputBlockId,
                      float aPresShellResolution);
  void ProcessLongTapUp(const CSSPoint& aPoint,
                        const ScrollableLayerGuid& aGuid,
                        float aPresShellResolution);
  void ProcessTouchEvent(const WidgetTouchEvent& aEvent,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId);
  void ProcessWheelEvent(const WidgetWheelEvent& aEvent,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId);
  void ProcessAPZStateChange(const nsCOMPtr<nsIDocument>& aDocument,
                             ViewID aViewId,
                             APZStateChange aChange,
                             int aArg);
private:
  ~APZEventState();
  void SendPendingTouchPreventedResponse(bool aPreventDefault,
                                         const ScrollableLayerGuid& aGuid);
  already_AddRefed<nsIWidget> GetWidget() const;
private:
  nsWeakPtr mWidget;
  nsRefPtr<ActiveElementManager> mActiveElementManager;
  nsRefPtr<ContentReceivedInputBlockCallback> mContentReceivedInputBlockCallback;
  bool mPendingTouchPreventedResponse;
  ScrollableLayerGuid mPendingTouchPreventedGuid;
  uint64_t mPendingTouchPreventedBlockId;
  bool mEndTouchIsClick;
  bool mTouchEndCancelled;
};

}
}

#endif /* mozilla_layers_APZEventState_h */
