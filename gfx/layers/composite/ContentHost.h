/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTENTHOST_H
#define GFX_CONTENTHOST_H

#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for FILE
#include "mozilla-config.h"             // for MOZ_DUMP_PAINTING
#include "CompositableHost.h"           // for CompositableHost, etc
#include "RotatedBuffer.h"              // for RotatedContentBuffer, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Filter
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayersTypes.h"  // for etc
#include "mozilla/layers/TextureHost.h"  // for TextureHost
#include "mozilla/mozalloc.h"           // for operator delete
#include "mozilla/UniquePtr.h"          // for UniquePtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray
#include "nscore.h"                     // for nsACString

namespace mozilla {
namespace gfx {
class Matrix4x4;
}
namespace layers {
class Compositor;
class ThebesBufferData;
class TiledLayerComposer;
struct EffectChain;
class TextureImageTextureSourceOGL;

struct TexturedEffect;

/**
 * ContentHosts are used for compositing Painted layers, always matched by a
 * ContentClient of the same type.
 *
 * ContentHosts support only UpdateThebes(), not Update().
 */
class ContentHost : public CompositableHost
{
public:
  // Subclasses should implement this method if they support being used as a
  // tiling.
  virtual TiledLayerComposer* AsTiledLayerComposer() { return nullptr; }

  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack) = 0;

  virtual void SetPaintWillResample(bool aResample) { mPaintWillResample = aResample; }
  bool PaintWillResample() { return mPaintWillResample; }

protected:
  explicit ContentHost(const TextureInfo& aTextureInfo)
    : CompositableHost(aTextureInfo)
    , mPaintWillResample(false)
  {}

  bool mPaintWillResample;
};

/**
 * Base class for non-tiled ContentHosts.
 *
 * Ownership of the SurfaceDescriptor and the resources it represents is passed
 * from the ContentClient to the ContentHost when the TextureClient/Hosts are
 * created, that is recevied here by SetTextureHosts which assigns one or two
 * texture hosts (for single and double buffering) to the ContentHost.
 *
 * It is the responsibility of the ContentHost to destroy its resources when
 * they are recreated or the ContentHost dies.
 */
class ContentHostBase : public ContentHost
{
public:
  typedef RotatedContentBuffer::ContentType ContentType;
  typedef RotatedContentBuffer::PaintState PaintState;

  explicit ContentHostBase(const TextureInfo& aTextureInfo);
  virtual ~ContentHostBase();

protected:
  virtual nsIntPoint GetOriginOffset()
  {
    return mBufferRect.TopLeft() - mBufferRotation;
  }


  nsIntRect mBufferRect;
  nsIntPoint mBufferRotation;
  bool mInitialised;
};

/**
 * Shared ContentHostBase implementation for content hosts that
 * use up to two TextureHosts.
 */
class ContentHostTexture : public ContentHostBase
{
public:
  explicit ContentHostTexture(const TextureInfo& aTextureInfo)
    : ContentHostBase(aTextureInfo)
    , mLocked(false)
  { }

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr) override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix="",
                    bool aDumpHtml=false) override;

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void UseTextureHost(TextureHost* aTexture) override;
  virtual void UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                         TextureHost* aTextureOnWhite) override;

  virtual bool Lock() override {
    MOZ_ASSERT(!mLocked);
    if (!mTextureHost) {
      return false;
    }
    if (!mTextureHost->Lock()) {
      return false;
    }

    if (mTextureHostOnWhite && !mTextureHostOnWhite->Lock()) {
      return false;
    }

    mLocked = true;
    return true;
  }
  virtual void Unlock() override {
    MOZ_ASSERT(mLocked);
    mTextureHost->Unlock();
    if (mTextureHostOnWhite) {
      mTextureHostOnWhite->Unlock();
    }
    mLocked = false;
  }

  LayerRenderState GetRenderState() override;

  virtual already_AddRefed<TexturedEffect> GenEffect(const gfx::Filter& aFilter) override;

protected:
  CompositableTextureHostRef mTextureHost;
  CompositableTextureHostRef mTextureHostOnWhite;
  CompositableTextureSourceRef mTextureSource;
  CompositableTextureSourceRef mTextureSourceOnWhite;
  bool mLocked;
};

/**
 * Double buffering is implemented by swapping the front and back TextureHosts.
 * We assume that whenever we use double buffering, then we have
 * render-to-texture and thus no texture upload to do.
 */
class ContentHostDoubleBuffered : public ContentHostTexture
{
public:
  explicit ContentHostDoubleBuffered(const TextureInfo& aTextureInfo)
    : ContentHostTexture(aTextureInfo)
  {}

  virtual ~ContentHostDoubleBuffered() {}

  virtual CompositableType GetType() { return CompositableType::CONTENT_DOUBLE; }

  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack);

protected:
  nsIntRegion mValidRegionForNextBackBuffer;
};

/**
 * Single buffered, therefore we must synchronously upload the image from the
 * TextureHost in the layers transaction (i.e., in UpdateThebes).
 */
class ContentHostSingleBuffered : public ContentHostTexture
{
public:
  explicit ContentHostSingleBuffered(const TextureInfo& aTextureInfo)
    : ContentHostTexture(aTextureInfo)
  {}
  virtual ~ContentHostSingleBuffered() {}

  virtual CompositableType GetType() { return CompositableType::CONTENT_SINGLE; }

  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack);
};

/**
 * Maintains a host-side only texture, and gets provided with
 * surfaces that only cover the changed pixels during an update.
 *
 * Takes ownership of the passed in update surfaces, and must
 * free them once texture upload is complete.
 *
 * Delays texture uploads until the next composite to
 * avoid blocking the main thread.
 */
class ContentHostIncremental : public ContentHostBase
{
public:
  explicit ContentHostIncremental(const TextureInfo& aTextureInfo);
  ~ContentHostIncremental();

  virtual CompositableType GetType() override { return CompositableType::CONTENT_INC; }

  virtual LayerRenderState GetRenderState() override { return LayerRenderState(); }

  virtual bool CreatedIncrementalTexture(ISurfaceAllocator* aAllocator,
                                         const TextureInfo& aTextureInfo,
                                         const nsIntRect& aBufferRect) override;

  virtual void UpdateIncremental(TextureIdentifier aTextureId,
                                 SurfaceDescriptor& aSurface,
                                 const nsIntRegion& aUpdated,
                                 const nsIntRect& aBufferRect,
                                 const nsIntPoint& aBufferRotation) override;

  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack) override
  {
    NS_ERROR("Shouldn't call this");
    return false;
  }

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr) override;

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual bool Lock() override {
    MOZ_ASSERT(!mLocked);
    ProcessTextureUpdates();
    mLocked = true;
    return true;
  }

  virtual void Unlock() override {
    MOZ_ASSERT(mLocked);
    mLocked = false;
  }

  virtual already_AddRefed<TexturedEffect>
  GenEffect(const gfx::Filter& aFilter) override;

private:

  void FlushUpdateQueue();
  void ProcessTextureUpdates();

  class Request
  {
  public:
    Request()
    {
      MOZ_COUNT_CTOR(ContentHostIncremental::Request);
    }

    virtual ~Request()
    {
      MOZ_COUNT_DTOR(ContentHostIncremental::Request);
    }

    virtual void Execute(ContentHostIncremental *aHost) = 0;
  };

  class TextureCreationRequest : public Request
  {
  public:
    TextureCreationRequest(const TextureInfo& aTextureInfo,
                           const nsIntRect& aBufferRect)
      : mTextureInfo(aTextureInfo)
      , mBufferRect(aBufferRect)
    {}

    virtual void Execute(ContentHostIncremental *aHost);

  private:
    TextureInfo mTextureInfo;
    nsIntRect mBufferRect;
  };

  class TextureUpdateRequest : public Request
  {
  public:
    TextureUpdateRequest(ISurfaceAllocator* aDeAllocator,
                         TextureIdentifier aTextureId,
                         SurfaceDescriptor& aDescriptor,
                         const nsIntRegion& aUpdated,
                         const nsIntRect& aBufferRect,
                         const nsIntPoint& aBufferRotation)
      : mDeAllocator(aDeAllocator)
      , mTextureId(aTextureId)
      , mDescriptor(aDescriptor)
      , mUpdated(aUpdated)
      , mBufferRect(aBufferRect)
      , mBufferRotation(aBufferRotation)
    {}

    ~TextureUpdateRequest()
    {
      //TODO: Recycle these?
      mDeAllocator->DestroySharedSurface(&mDescriptor);
    }

    virtual void Execute(ContentHostIncremental *aHost);

  private:
    enum XSide {
      LEFT, RIGHT
    };
    enum YSide {
      TOP, BOTTOM
    };

    nsIntRect GetQuadrantRectangle(XSide aXSide, YSide aYSide) const;

    RefPtr<ISurfaceAllocator> mDeAllocator;
    TextureIdentifier mTextureId;
    SurfaceDescriptor mDescriptor;
    nsIntRegion mUpdated;
    nsIntRect mBufferRect;
    nsIntPoint mBufferRotation;
  };

  nsTArray<UniquePtr<Request> > mUpdateList;

  // Specific to OGL to avoid exposing methods on TextureSource that only
  // have one implementation.
  RefPtr<TextureImageTextureSourceOGL> mSource;
  RefPtr<TextureImageTextureSourceOGL> mSourceOnWhite;

  RefPtr<ISurfaceAllocator> mDeAllocator;
  bool mLocked;
};

}
}

#endif
