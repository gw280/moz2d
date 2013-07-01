/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACENVPR_H_
#define MOZILLA_GFX_SOURCESURFACENVPR_H_

#include "2D.h"
#include "GLContextNVpr.h"
#include <mozilla/WeakPtr.h>
#include <vector>

namespace mozilla {
namespace gfx {

class DataSourceSurfaceNVpr;

class SourceSurfaceNVpr : public SourceSurface
{
  friend class DataSourceSurfaceNVpr;

public:
  static TemporaryRef<SourceSurfaceNVpr>
  CreateFromData(DataSourceSurface* aData);

  static TemporaryRef<SourceSurfaceNVpr>
  CreateFromData(SurfaceFormat aFormat, const IntSize& aSize,
                 const GLvoid* aData, GLsizei aStride = 0);

  static TemporaryRef<SourceSurfaceNVpr>
  CreateFromFramebuffer(SurfaceFormat aFormat, const IntSize& aSize);

  ~SourceSurfaceNVpr();

  GLsizei BytesPerPixel() const { return mBytesPerPixel; }
  operator GLuint() const { return mTextureId; }

  void ApplyTexturingOptions(Filter aFilter, ExtendMode aExtendMode,
                             SamplingBounds aSamplingBounds = SAMPLING_UNBOUNDED);

  virtual SurfaceType GetType() const { return SURFACE_NVPR_TEXTURE; }
  virtual IntSize GetSize() const { return mSize; }
  virtual SurfaceFormat GetFormat() const { return mFormat; }

  virtual TemporaryRef<DataSourceSurface> GetDataSurface();


private:
  SourceSurfaceNVpr(SurfaceFormat aFormat, const IntSize& aSize,
                    bool& aSuccess);

  void WritePixels(const GLvoid* aData, GLsizei aStride = 0);
  void ReadPixels(GLvoid* aBuffer);

  const SurfaceFormat mFormat;
  const IntSize mSize;
  GLenum mGLFormat;
  GLenum mGLType;
  GLsizei mBytesPerPixel;
  GLuint mTextureId;
  Filter mFilter;
  ExtendMode mExtendMode;
  bool mHasMipmaps;
  WeakPtr<DataSourceSurfaceNVpr> mDataSurface;
};


class DataSourceSurfaceNVpr
  : public DataSourceSurface
  , public SupportsWeakPtr<DataSourceSurfaceNVpr>
{
public:
  DataSourceSurfaceNVpr(TemporaryRef<SourceSurfaceNVpr> aSourceSurface)
    : mSourceSurface(aSourceSurface)
  {}

  virtual IntSize GetSize() const { return mSourceSurface->GetSize(); }
  virtual SurfaceFormat GetFormat() const { return mSourceSurface->GetFormat(); }

  virtual unsigned char* GetData();
  virtual int32_t Stride();

  virtual void MarkDirty();

private:
  RefPtr<SourceSurfaceNVpr> mSourceSurface;
  std::vector<GLubyte> mShadowBuffer;
};

}
}

#endif /* MOZILLA_GFX_SOURCESURFACENVPR_H_ */
