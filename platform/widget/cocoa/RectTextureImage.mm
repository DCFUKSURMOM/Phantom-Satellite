/* -*- Mode: objc; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RectTextureImage.h"

#include "gfxUtils.h"
#include "GLContextCGL.h"
#include "mozilla/layers/GLManager.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "OGLShaderProgram.h"
#include "ScopedGLHelpers.h"
#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)
#include "GLUploadHelpers.h"
#endif

#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)
#include "mozilla/gfx/SourceSurfaceCG.h"
#endif

namespace mozilla {
namespace widget {

#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
RectTextureImage::RectTextureImage()
  : mGLContext(nullptr)
#else
RectTextureImage::RectTextureImage(gl::GLContext* aGLContext)
  : mGLContext(aGLContext)
#endif
  , mTexture(0)
  , mInUpdate(false)
{
}

#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)

/**
 * Returns the first integer greater than |aNumber| which is a power of two
 * Undefined for |aNumber| < 0
 */
static int
NextPowerOfTwo(int aNumber)
{
#if defined(__arm__)
    return 1 << (32 - __builtin_clz(aNumber - 1));
#else
    --aNumber;
    aNumber |= aNumber >> 1;
    aNumber |= aNumber >> 2;
    aNumber |= aNumber >> 4;
    aNumber |= aNumber >> 8;
    aNumber |= aNumber >> 16;
    return ++aNumber;
#endif
}

LayoutDeviceIntSize
RectTextureImage::TextureSizeForSize(const LayoutDeviceIntSize& aSize)
{
  return LayoutDeviceIntSize(NextPowerOfTwo(aSize.width),
                             NextPowerOfTwo(aSize.height));
}
#endif

RectTextureImage::~RectTextureImage()
{
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  DeleteTexture();
#else
  if (mTexture) {
    mGLContext->MakeCurrent();
    mGLContext->fDeleteTextures(1, &mTexture);
    mTexture = 0;
  }
#endif
}

already_AddRefed<gfx::DrawTarget>
RectTextureImage::BeginUpdate(const LayoutDeviceIntSize& aNewSize,
                              const LayoutDeviceIntRegion& aDirtyRegion)
{
  MOZ_ASSERT(!mInUpdate, "Beginning update during update!");
  mUpdateRegion = aDirtyRegion;
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  bool needRecreate = false;
  if (aNewSize != mBufferSize) {
    mBufferSize = aNewSize;
#else
  if (aNewSize != mUsedSize) {
    mUsedSize = aNewSize;
#endif
    mUpdateRegion =
      LayoutDeviceIntRect(LayoutDeviceIntPoint(0, 0), aNewSize);
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
    needRecreate = true;
#endif
  }

  if (mUpdateRegion.IsEmpty()) {
    return nullptr;
  }

#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  if (!mIOSurface || needRecreate) {
    DeleteTexture();
    mIOSurface = MacIOSurface::CreateIOSurface(mBufferSize.width,
                                               mBufferSize.height);

    if (!mIOSurface) {
      return nullptr;
    }
#else
  LayoutDeviceIntSize neededBufferSize = TextureSizeForSize(mUsedSize);
  if (!mUpdateDrawTarget || mBufferSize != neededBufferSize) {
    gfx::IntSize size(neededBufferSize.width, neededBufferSize.height);
    mUpdateDrawTarget = nullptr;
    mUpdateDrawTargetData = nullptr;
    gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
    int32_t stride = size.width * gfx::BytesPerPixel(format);
    mUpdateDrawTargetData = MakeUnique<unsigned char[]>(stride * size.height);
    mUpdateDrawTarget =
      gfx::Factory::CreateDrawTargetForData(gfx::BackendType::COREGRAPHICS,
                                            mUpdateDrawTargetData.get(), size,
                                            stride, format);
    mBufferSize = neededBufferSize;

#endif
  }

  mInUpdate = true;

#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  mIOSurface->Lock(false);
  unsigned char* ioData = (unsigned char*)mIOSurface->GetBaseAddress();
  gfx::IntSize size(mBufferSize.width, mBufferSize.height);
  int32_t stride = mIOSurface->GetBytesPerRow();
  gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
  RefPtr<gfx::DrawTarget> drawTarget =
    gfx::Factory::CreateDrawTargetForData(gfx::BackendType::SKIA,
                                          ioData, size,
                                          stride, format);
#else
  RefPtr<gfx::DrawTarget> drawTarget = mUpdateDrawTarget;
#endif
  return drawTarget.forget();
}

#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)
static bool
CanUploadSubtextures()
{
  return false;
}
#endif


void
RectTextureImage::EndUpdate(bool aKeepSurface)
{
  MOZ_ASSERT(mInUpdate, "Ending update while not in update");
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  mIOSurface->Unlock(false);
#else
  mGLContext->MakeCurrent();
  bool needInit = !mTexture;
  if (!mTexture) {
    mGLContext->fGenTextures(1, &mTexture);
  }
  LayoutDeviceIntRegion updateRegion = mUpdateRegion;
  if (mTextureSize != mBufferSize) {
    mTextureSize = mBufferSize;
    needInit = true;
  }

  if (needInit || !CanUploadSubtextures()) {
    updateRegion =
      LayoutDeviceIntRect(LayoutDeviceIntPoint(0, 0), mTextureSize);
  }

  gfx::IntPoint srcPoint = updateRegion.GetBounds().TopLeft().ToUnknownPoint();
  gfx::SurfaceFormat format = mUpdateDrawTarget->GetFormat();
  int bpp = gfx::BytesPerPixel(format);
  int32_t stride = mBufferSize.width * bpp;
  unsigned char* data = mUpdateDrawTargetData.get();
  data += srcPoint.y * stride + srcPoint.x * bpp;

  UploadImageDataToTexture(mGLContext, data, stride, format,
                           updateRegion.ToUnknownRegion(), mTexture,
                           mTextureSize.ToUnknownSize(), nullptr, needInit,
                           LOCAL_GL_TEXTURE0,
                           LOCAL_GL_TEXTURE_RECTANGLE_ARB);


  if (!aKeepSurface) {
    mUpdateDrawTarget = nullptr;
    mUpdateDrawTargetData = nullptr;
  }
#endif
  mInUpdate = false;
}

void
RectTextureImage::UpdateFromCGContext(const LayoutDeviceIntSize& aNewSize,
                                      const LayoutDeviceIntRegion& aDirtyRegion,
                                      CGContextRef aCGContext)
{
  gfx::IntSize size = gfx::IntSize(CGBitmapContextGetWidth(aCGContext),
                                   CGBitmapContextGetHeight(aCGContext));

  RefPtr<gfx::DrawTarget> dt = BeginUpdate(aNewSize, aDirtyRegion);
  if (dt) {
    gfx::Rect rect(0, 0, size.width, size.height);
    gfxUtils::ClipToRegion(dt, GetUpdateRegion().ToUnknownRegion());
    unsigned char *data = (unsigned char *)CGBitmapContextGetData(aCGContext);
    if (data) {
      RefPtr<gfx::SourceSurface> sourceSurface =
        dt->CreateSourceSurfaceFromData(data,
                                        size,
                                        CGBitmapContextGetBytesPerRow(aCGContext),
                                        gfx::SurfaceFormat::B8G8R8A8);
      dt->DrawSurface(sourceSurface, rect, rect,
                      gfx::DrawSurfaceOptions(),
                      gfx::DrawOptions(1.0, gfx::CompositionOp::OP_SOURCE));
    }
#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6) 
    else {
        CGImageRef image = CGBitmapContextCreateImage(aCGContext);
        if (image) {
            RefPtr<gfx::SourceSurface> sourceSurface = new gfx::SourceSurfaceCG(image);

            dt->DrawSurface(sourceSurface, rect, rect,
                            gfx::DrawSurfaceOptions(),
                            gfx::DrawOptions(1.0, gfx::CompositionOp::OP_SOURCE));
        }
    }
#endif
    dt->PopClip();
    EndUpdate(true);
  }
}



void
RectTextureImage::Draw(layers::GLManager* aManager,
                       const LayoutDeviceIntPoint& aLocation,
                       const gfx::Matrix4x4& aTransform)
{
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  gl::GLContext* gl = aManager->gl();

  BindIOSurfaceToTexture(gl);
#endif

  layers::ShaderProgramOGL* program =
    aManager->GetProgram(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                         gfx::SurfaceFormat::R8G8B8A8);

#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl::ScopedBindTexture texture(gl, mTexture, LOCAL_GL_TEXTURE_RECTANGLE_ARB);
#else
  aManager->gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  aManager->gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, mTexture);
#endif

  aManager->ActivateProgram(program);
  program->SetProjectionMatrix(aManager->GetProjMatrix());
  program->SetLayerTransform(gfx::Matrix4x4(aTransform).PostTranslate(aLocation.x, aLocation.y, 0));
  program->SetTextureTransform(gfx::Matrix4x4());
  program->SetRenderOffset(nsIntPoint(0, 0));
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  program->SetTexCoordMultiplier(mBufferSize.width, mBufferSize.height);
#else
  program->SetTexCoordMultiplier(mUsedSize.width, mUsedSize.height);
#endif
  program->SetTextureUnit(0);

  aManager->BindAndDrawQuad(program,
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
                            gfx::Rect(0.0, 0.0, mBufferSize.width, mBufferSize.height),
#else
                            gfx::Rect(0.0, 0.0, mUsedSize.width, mUsedSize.height),
#endif
                            gfx::Rect(0.0, 0.0, 1.0f, 1.0f));

#if !defined(MAC_OS_X_VERSION_10_6) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)
  aManager->gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
#endif
}

#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
void
RectTextureImage::DeleteTexture()
{
  if (mTexture) {
    MOZ_ASSERT(mGLContext);
    mGLContext->MakeCurrent();
    mGLContext->fDeleteTextures(1, &mTexture);
    mTexture = 0;
  }
}

void
RectTextureImage::BindIOSurfaceToTexture(gl::GLContext* aGL)
{
  if (!mTexture && mIOSurface) {
    MOZ_ASSERT(aGL);
    aGL->fGenTextures(1, &mTexture);
    aGL->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl::ScopedBindTexture texture(aGL, mTexture, LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_MIN_FILTER,
                        LOCAL_GL_LINEAR);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_MAG_FILTER,
                        LOCAL_GL_LINEAR);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_WRAP_T,
                        LOCAL_GL_CLAMP_TO_EDGE);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_WRAP_S,
                        LOCAL_GL_CLAMP_TO_EDGE);

    mIOSurface->CGLTexImageIOSurface2D(gl::GLContextCGL::Cast(aGL)->GetCGLContext());
    mGLContext = aGL;
  }
}
#endif

} // namespace widget
} // namespace mozilla
