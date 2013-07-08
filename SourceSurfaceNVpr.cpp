/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceNVpr.h"

using namespace mozilla::gfx::nvpr;
using namespace std;

namespace mozilla {
namespace gfx {

SourceSurfaceNVpr::SourceSurfaceNVpr(SurfaceFormat aFormat, const IntSize& aSize,
                                     bool& aSuccess)
  : mFormat(aFormat)
  , mSize(aSize)
  , mTextureId(0)
  , mFilter(FILTER_LINEAR)
  , mExtendMode(EXTEND_REPEAT)
  , mHasMipmaps(false)
{
  MOZ_ASSERT(mSize.width >= 0 && mSize.height >= 0);

  aSuccess = false;

  gl->MakeCurrent();

  if (max(mSize.width, mSize.height) > gl->MaxTextureSize()) {
    return;
  }

  gl->GenTextures(1, &mTextureId);

  GLenum internalFormat;
  switch (mFormat) {
    case FORMAT_YUV:
    case FORMAT_UNKNOWN:
    default:
      return;
    case FORMAT_A8:
      // TODO: Use use GL_RED and have a shader treat it as alpha.
      internalFormat = GL_RGBA8; // Grrr. GL_ALPHA was deprecated in OpenGL 3.
      mGLFormat = GL_UNSIGNED_BYTE;
      mGLType = GL_ALPHA;
      mBytesPerPixel = 1;
    case FORMAT_B8G8R8A8:
      internalFormat = GL_RGBA8;
      mGLFormat = GL_BGRA;
      mGLType = GL_UNSIGNED_BYTE;
      mBytesPerPixel = 4;
      break;
    case FORMAT_B8G8R8X8:
      internalFormat = GL_RGB8;
      mGLFormat = GL_BGRA;
      mGLType = GL_UNSIGNED_BYTE;
      mBytesPerPixel = 4;
      break;
    case FORMAT_R8G8B8A8:
      internalFormat = GL_RGBA8;
      mGLFormat = GL_RGBA;
      mGLType = GL_UNSIGNED_BYTE;
      mBytesPerPixel = 4;
      break;
    case FORMAT_R8G8B8X8:
      internalFormat = GL_RGB8;
      mGLFormat = GL_RGBA;
      mGLType = GL_UNSIGNED_BYTE;
      mBytesPerPixel = 4;
      break;
    case FORMAT_R5G6B5:
      internalFormat = GL_RGB565;
      mGLFormat = GL_RGB;
      mGLType = GL_UNSIGNED_SHORT_5_6_5;
      mBytesPerPixel = 2;
      break;
  }

  gl->TextureImage2DEXT(mTextureId, GL_TEXTURE_2D, 0, internalFormat, mSize.width,
                      mSize.height, 0, mGLFormat, mGLType, nullptr);

  // The initial value for MIN_FILTER is NEAREST_MIPMAP_LINEAR. We initialize it
  // to what 'FILTER_LINEAR' expects.
  gl->TextureParameteriEXT(mTextureId, GL_TEXTURE_2D,
                           GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  if (gl->HasExtension(GL::EXT_texture_filter_anisotropic)) {
    gl->TextureParameteriEXT(mTextureId, GL_TEXTURE_2D,
                             GL_TEXTURE_MAX_ANISOTROPY_EXT, gl->MaxAnisotropy());
  }

  aSuccess = true;
}

TemporaryRef<SourceSurfaceNVpr>
SourceSurfaceNVpr::CreateFromData(DataSourceSurface* aData)
{
  return CreateFromData(aData->GetFormat(), aData->GetSize(),
                        aData->GetData(), aData->Stride());
}

TemporaryRef<SourceSurfaceNVpr>
SourceSurfaceNVpr::CreateFromData(SurfaceFormat aFormat, const IntSize& aSize,
                                  const GLvoid* aData, GLsizei aStride)
{
  bool success;
  RefPtr<SourceSurfaceNVpr> surface
   = new SourceSurfaceNVpr(aFormat, aSize, success);
  if (!success) {
   return nullptr;
  }

  surface->WritePixels(aData, aStride);

  return surface.forget();
}

TemporaryRef<SourceSurfaceNVpr>
SourceSurfaceNVpr::CreateFromFramebuffer(SurfaceFormat aFormat, const IntSize& aSize)
{
  bool success;
  RefPtr<SourceSurfaceNVpr> surface = new SourceSurfaceNVpr(aFormat, aSize, success);
  if (!success) {
   return nullptr;
  }

  MOZ_ASSERT(gl->IsCurrent());

  gl->SetFramebufferToTexture(GL_DRAW_FRAMEBUFFER, GL_TEXTURE_2D, *surface);

  gl->BlitFramebuffer(0, surface->mSize.height, surface->mSize.width, 0,
                      0, 0, surface->mSize.width, surface->mSize.height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

  return surface.forget();
}

SourceSurfaceNVpr::~SourceSurfaceNVpr()
{
  gl->MakeCurrent();
  gl->DeleteTexture(mTextureId);
}

void
SourceSurfaceNVpr::ApplyTexturingOptions(Filter aFilter, ExtendMode aExtendMode,
                                         SamplingBounds aSamplingBounds)
{
  MOZ_ASSERT(gl->IsCurrent());

  if (mFilter != aFilter) {
    GLenum minFilter;
    GLenum magFilter;
    GLint anisotropy;
    switch (aFilter) {
    default:
      MOZ_ASSERT(!"Invalid filter");
    case FILTER_LINEAR:
      minFilter = GL_LINEAR_MIPMAP_LINEAR;
      magFilter = GL_LINEAR;
      anisotropy = gl->MaxAnisotropy();
      break;
    case FILTER_POINT:
      minFilter = magFilter = GL_NEAREST;
      anisotropy = 1;
      break;
    }

    gl->TextureParameteriEXT(mTextureId, GL_TEXTURE_2D,
                             GL_TEXTURE_MIN_FILTER, minFilter);
    gl->TextureParameteriEXT(mTextureId, GL_TEXTURE_2D,
                             GL_TEXTURE_MAG_FILTER, magFilter);

    if (gl->HasExtension(GL::EXT_texture_filter_anisotropic)) {
      gl->TextureParameteriEXT(mTextureId, GL_TEXTURE_2D,
                               GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
    }

    mFilter = aFilter;
  }

  if (mFilter == FILTER_LINEAR && !mHasMipmaps) {
    gl->GenerateTextureMipmapEXT(mTextureId, GL_TEXTURE_2D);
    mHasMipmaps = true;
  }

  if (mExtendMode != aExtendMode) {
    GLenum wrapMode;
    switch (aExtendMode) {
      default:
        MOZ_ASSERT(!"Invalid extend mode");
      case EXTEND_CLAMP:
        wrapMode = GL_CLAMP_TO_EDGE;
        break;
      case EXTEND_REPEAT:
        wrapMode = GL_REPEAT;
        break;
      case EXTEND_REFLECT:
        wrapMode = GL_MIRRORED_REPEAT;
        break;
    }

    gl->TextureParameteriEXT(mTextureId, GL_TEXTURE_2D,
                             GL_TEXTURE_WRAP_S, wrapMode);
    gl->TextureParameteriEXT(mTextureId, GL_TEXTURE_2D,
                             GL_TEXTURE_WRAP_T, wrapMode);

    mExtendMode = aExtendMode;
  }

  if (aSamplingBounds == SAMPLING_BOUNDED) {
    // TODO: Use a shader to clamp to the middle of the outer pixels.
  }
}

void
SourceSurfaceNVpr::WritePixels(const GLvoid* aData, GLsizei aStride)
{
  const GLsizei bytesPerRow = mSize.width * mBytesPerPixel;
  const GLubyte* pixelData = static_cast<const GLubyte*>(aData);
  vector<GLubyte> packBuffer;

  gl->MakeCurrent();

  if (aStride == 0 || aStride == bytesPerRow) {
    gl->PixelStorei(GL_PACK_ALIGNMENT, 1);
  } else if (aStride == bytesPerRow + 2 - (bytesPerRow % 2)) {
    gl->PixelStorei(GL_PACK_ALIGNMENT, 2);
  } else if (aStride == bytesPerRow + 4 - (bytesPerRow % 4)) {
    gl->PixelStorei(GL_PACK_ALIGNMENT, 4);
  } else if (aStride == bytesPerRow + 8 - (bytesPerRow % 8)) {
    gl->PixelStorei(GL_PACK_ALIGNMENT, 8);
  } else {
    packBuffer.resize(mSize.height * bytesPerRow);
    for (int i = 0; i < mSize.height; i++) {
      memcpy(&packBuffer[i * bytesPerRow], &pixelData[i * aStride], bytesPerRow);
    }
    pixelData = packBuffer.data();
    gl->PixelStorei(GL_PACK_ALIGNMENT, 1);
  }

  gl->TextureSubImage2DEXT(mTextureId, GL_TEXTURE_2D, 0, 0, 0, mSize.width,
                           mSize.height, mGLFormat, mGLType, pixelData);

  mHasMipmaps = false;
}

void
SourceSurfaceNVpr::ReadPixels(GLvoid* aBuffer)
{
  gl->MakeCurrent();
  gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
  gl->GetTextureImageEXT(mTextureId, GL_TEXTURE_2D, 0, mGLFormat, mGLType, aBuffer);
}

TemporaryRef<DataSourceSurface>
SourceSurfaceNVpr::GetDataSurface()
{
  if (mDataSurface)
    return mDataSurface.get();

  RefPtr<DataSourceSurfaceNVpr> dataSurface = new DataSourceSurfaceNVpr(this);
  mDataSurface = dataSurface->asWeakPtr();

  return dataSurface.forget();
}

unsigned char*
DataSourceSurfaceNVpr::GetData()
{
  if (mShadowBuffer.empty()) {
    mShadowBuffer.resize(mSourceSurface->GetSize().height * Stride());
    mSourceSurface->ReadPixels(mShadowBuffer.data());
  }

  return mShadowBuffer.data();
}

int32_t
DataSourceSurfaceNVpr::Stride()
{
  return mSourceSurface->GetSize().width * mSourceSurface->BytesPerPixel();
}

void
DataSourceSurfaceNVpr::MarkDirty()
{
  if (mShadowBuffer.empty()) {
    return;
  }

  mSourceSurface->WritePixels(mShadowBuffer.data());
}

}
}
