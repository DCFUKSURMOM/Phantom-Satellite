/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RectTextureImage_h_
#define RectTextureImage_h_

#include "mozilla/RefPtr.h"
#include <functional>
#include <utility>

class MacIOSurface;

namespace mozilla {

namespace gl {
class GLContext;
} // namespace gl

namespace widget {

// Manages a texture which can resize dynamically, binds to the
// LOCAL_GL_TEXTURE_RECTANGLE_ARB texture target and is automatically backed
// by a power-of-two size GL texture. The latter two features are used for
// compatibility with older Mac hardware which we block GL layers on.
// RectTextureImages are used both for accelerated GL layers drawing and for
// OMTC BasicLayers drawing.
class RectTextureImage {
public:

// Back out 1281686 when we do not have IOSurface
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  RectTextureImage();
#else
  explicit RectTextureImage(gl::GLContext* aGLContext);
#endif

  virtual ~RectTextureImage();

  already_AddRefed<gfx::DrawTarget>
    BeginUpdate(const LayoutDeviceIntSize& aNewSize,
                const LayoutDeviceIntRegion& aDirtyRegion =
                  LayoutDeviceIntRegion());
  void EndUpdate(bool aKeepSurface = false);

  template<typename Function, typename... Args>
  void UpdateIfNeeded(const LayoutDeviceIntSize& aNewSize,
                      const LayoutDeviceIntRegion& aDirtyRegion,
                      Function aCallback,
                      Args&&... aArgs)
  {
    RefPtr<gfx::DrawTarget> drawTarget = BeginUpdate(aNewSize, aDirtyRegion);
    if (drawTarget) {
      aCallback(drawTarget, GetUpdateRegion(), std::forward<Args>(aArgs)...);
      EndUpdate();
    }
  }

  void UpdateFromCGContext(const LayoutDeviceIntSize& aNewSize,
                           const LayoutDeviceIntRegion& aDirtyRegion,
                           CGContextRef aCGContext);

  LayoutDeviceIntRegion GetUpdateRegion() {
    MOZ_ASSERT(mInUpdate, "update region only valid during update");
    return mUpdateRegion;
  }

  void Draw(mozilla::layers::GLManager* aManager,
            const LayoutDeviceIntPoint& aLocation,
            const gfx::Matrix4x4& aTransform = gfx::Matrix4x4());

#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)
  static LayoutDeviceIntSize TextureSizeForSize(
    const LayoutDeviceIntSize& aSize);
#endif

protected:
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  void DeleteTexture();
  void BindIOSurfaceToTexture(gl::GLContext* aGL);
#endif

#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  RefPtr<MacIOSurface> mIOSurface;
#else
  RefPtr<gfx::DrawTarget> mUpdateDrawTarget;
  UniquePtr<unsigned char[]> mUpdateDrawTargetData;
#endif
  gl::GLContext* mGLContext;
  LayoutDeviceIntRegion mUpdateRegion;
#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)
  LayoutDeviceIntSize mUsedSize;
#endif
  LayoutDeviceIntSize mBufferSize;
#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)
  LayoutDeviceIntSize mTextureSize;
#endif
  GLuint mTexture;
  bool mInUpdate;
};

} // namespace widget
} // namespace mozilla

#endif // RectTextureImage_h_
