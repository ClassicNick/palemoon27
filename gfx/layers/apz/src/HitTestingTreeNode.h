/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_HitTestingTreeNode_h
#define mozilla_layers_HitTestingTreeNode_h

#include "APZUtils.h"                       // for HitTestResult
#include "FrameMetrics.h"                   // for ScrollableLayerGuid
#include "mozilla/gfx/Matrix.h"             // for Matrix4x4
#include "mozilla/layers/LayersTypes.h"     // for EventRegions
#include "mozilla/Maybe.h"                  // for Maybe
#include "mozilla/nsRefPtr.h"                       // for nsRefPtr

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

/**
 * This class represents a node in a tree that is used by the APZCTreeManager
 * to do hit testing. The tree is roughly a copy of the layer tree, but will
 * contain multiple nodes in cases where the layer has multiple FrameMetrics.
 * In other words, the structure of this tree should be identical to the
 * LayerMetrics tree (see documentation in LayerMetricsWrapper.h).
 *
 * Not all HitTestingTreeNode instances will have an APZC associated with them;
 * only HitTestingTreeNodes that correspond to layers with scrollable metrics
 * have APZCs.
 * Multiple HitTestingTreeNode instances may share the same underlying APZC
 * instance if the layers they represent share the same scrollable metrics (i.e.
 * are part of the same animated geometry root). If this happens, exactly one of
 * the HitTestingTreeNode instances will be designated as the "primary holder"
 * of the APZC. When this primary holder is destroyed, it will destroy the APZC
 * along with it; in contrast, destroying non-primary-holder nodes will not
 * destroy the APZC.
 * Code should not make assumptions about which of the nodes will be the
 * primary holder, only that that there will be exactly one for each APZC in
 * the tree.
 *
 * The reason this tree exists at all is so that we can do hit-testing on the
 * thread that we receive input on (referred to the as the controller thread in
 * APZ terminology), which may be different from the compositor thread.
 * Accessing the compositor layer tree can only be done on the compositor
 * thread, and so it is simpler to make a copy of the hit-testing related
 * properties into a separate tree.
 */
class HitTestingTreeNode {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HitTestingTreeNode);

private:
  ~HitTestingTreeNode();
public:
  HitTestingTreeNode(AsyncPanZoomController* aApzc, bool aIsPrimaryHolder);
  void RecycleWith(AsyncPanZoomController* aApzc);
  void Destroy();

  /* Tree construction methods */

  void SetLastChild(HitTestingTreeNode* aChild);
  void SetPrevSibling(HitTestingTreeNode* aSibling);
  void MakeRoot();

  /* Tree walking methods. GetFirstChild is O(n) in the number of children. The
   * other tree walking methods are all O(1). */

  HitTestingTreeNode* GetFirstChild() const;
  HitTestingTreeNode* GetLastChild() const;
  HitTestingTreeNode* GetPrevSibling() const;
  HitTestingTreeNode* GetParent() const;

  /* APZC related methods */

  AsyncPanZoomController* GetApzc() const;
  AsyncPanZoomController* GetNearestContainingApzc() const;
  bool IsPrimaryHolder() const;

  /* Hit test related methods */

  void SetHitTestData(const EventRegions& aRegions,
                      const gfx::Matrix4x4& aTransform,
                      const Maybe<nsIntRegion>& aClipRegion,
                      const EventRegionsOverride& aOverride);
  bool IsOutsideClip(const ParentLayerPoint& aPoint) const;
  /* Convert aPoint into the LayerPixel space for the layer corresponding to
   * this node. */
  Maybe<LayerPoint> Untransform(const ParentLayerPoint& aPoint) const;
  /* Assuming aPoint is inside the clip region for this node, check which of the
   * event region spaces it falls inside. */
  HitTestResult HitTest(const ParentLayerPoint& aPoint) const;
  /* Returns the mOverride flag. */
  EventRegionsOverride GetEventRegionsOverride() const;

  /* Debug helpers */
  void Dump(const char* aPrefix = "") const;

private:
  void SetApzcParent(AsyncPanZoomController* aApzc);

  nsRefPtr<HitTestingTreeNode> mLastChild;
  nsRefPtr<HitTestingTreeNode> mPrevSibling;
  nsRefPtr<HitTestingTreeNode> mParent;

  nsRefPtr<AsyncPanZoomController> mApzc;
  bool mIsPrimaryApzcHolder;

  /* Let {L,M} be the {layer, scrollable metrics} pair that this node
   * corresponds to in the layer tree. mEventRegions contains the event regions
   * from L, in the case where event-regions are enabled. If event-regions are
   * disabled, it will contain the visible region of L, which we use as an
   * approximation to the hit region for the purposes of obscuring other layers.
   * This value is in L's LayerPixels.
   */
  EventRegions mEventRegions;

  /* This is the transform from layer L. This does NOT include any async
   * transforms. */
  gfx::Matrix4x4 mTransform;

  /* This is clip rect for L that we wish to use for hit-testing purposes. Note
   * that this may not be exactly the same as the clip rect on layer L because
   * of the touch-sensitive region provided by the GoannaContentController, or
   * because we may use the composition bounds of the layer if the clip is not
   * present. This value is in L's ParentLayerPixels. */
  Maybe<nsIntRegion> mClipRegion;

  /* Indicates whether or not the event regions on this node need to be
   * overridden in a certain way. */
  EventRegionsOverride mOverride;
};

}
}

#endif // mozilla_layers_HitTestingTreeNode_h
