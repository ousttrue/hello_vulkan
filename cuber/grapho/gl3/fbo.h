#pragma once
#include "../camera/viewport.h"
#include "error_check.h"
#include "texture.h"
#include <assert.h>

namespace grapho {
namespace gl3 {

struct ClearParam
{
  bool Depth = true;
  bool ApplyAlpha = false;
};

void
ClearViewport(const camera::Viewport& vp, const ClearParam& param = {});

struct Fbo
{
  uint32_t m_fbo = 0;
  uint32_t m_rbo = 0;
  Fbo();
  ~Fbo();
  Fbo(const Fbo&) = delete;
  Fbo& operator=(const Fbo&) = delete;
  void Bind();
  void Unbind();
  void AttachDepth(int width, int height);
  void AttachTexture2D(uint32_t texture, int mipLevel = 0);
  void AttachCubeMap(int i, uint32_t texture, int mipLevel = 0);
};

struct FboHolder
{
  std::shared_ptr<grapho::gl3::Texture> FboTexture;
  grapho::gl3::Fbo Fbo;

  template<typename T>
  uint32_t Bind(int width, int height, const T& color)
  {
    static_assert(sizeof(T) == sizeof(float) * 4, "Bind");
    if (!FboTexture || FboTexture->Width() != width ||
        FboTexture->Height() != height) {
      FboTexture = grapho::gl3::Texture::Create({
        width,
        height,
        grapho::PixelFormat::u8_RGBA,

      });
      Fbo.AttachDepth(width, height);
      Fbo.AttachTexture2D(FboTexture->Handle());
    }

    Fbo.Bind();
    grapho::gl3::ClearViewport({
      .Width = (float)width,
      .Height = (float)height,
      .Color = *((const std::array<float, 4>*)&color),
    });

    return FboTexture->Handle();
  }

  void Unbind() { Fbo.Unbind(); }
};

struct RenderTarget
{
  std::shared_ptr<Fbo> Fbo;
  std::shared_ptr<Texture> FboTexture;

  uint32_t Begin(float width, float height, const float color[4])
  {
    if (width == 0 || height == 0) {
      return 0;
    }
    if (!Fbo) {
      Fbo = std::make_shared<grapho::gl3::Fbo>();
    }

    if (FboTexture) {
      if (FboTexture->Width() != width || FboTexture->Height() != height) {
        FboTexture = nullptr;
      }
    }
    if (!FboTexture) {
      FboTexture = grapho::gl3::Texture::Create({
        static_cast<int>(width),
        static_cast<int>(height),
        grapho::PixelFormat::u8_RGB,
        grapho::ColorSpace::Linear,
      });
      Fbo->AttachTexture2D(FboTexture->Handle());
      Fbo->AttachDepth(static_cast<int>(width), static_cast<int>(height));
    }

    Fbo->Bind();
    grapho::gl3::ClearViewport({
      .Width = width,
      .Height = height,
      .Color = { color[0], color[1], color[2], color[3] },
      .Depth = 1.0f,
    });

    return FboTexture->Handle();
  }

  void End() { Fbo->Unbind(); }
};

} // namespace
}
