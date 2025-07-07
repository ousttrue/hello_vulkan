#pragma once
#include "../image.h"
#include <memory>
#include <stdint.h>
#include <optional>

namespace grapho {
namespace gl3 {

std::optional<uint32_t>
GLImageFormat(PixelFormat format, ColorSpace colorspace);

uint32_t
GLInternalFormat(PixelFormat format);

class Texture
{
  uint32_t m_handle;
  int m_width = 0;
  int m_height = 0;

public:
  Texture();
  ~Texture();
  void Bind() const;
  void Unbind() const;
  void Activate(uint32_t unit) const;
  static void Deactivate(uint32_t unit);
  const uint32_t& Handle() const { return m_handle; }
  int Width() const { return m_width; }
  int Height() const { return m_height; }

  static std::shared_ptr<Texture> Create(const Image& data,
                                         bool useFloat = false)
  {
    auto ptr = std::shared_ptr<Texture>(new Texture());
    ptr->Upload(data, useFloat);
    return ptr;
  }

  void Upload(const Image& data, bool useFloat);

  void WrapClamp();

  void WrapRepeat();

  void SamplingPoint();

  void SamplingLinear(bool mip = false);
};

struct TextureSlot
{
  uint32_t Unit;
  std::shared_ptr<Texture> m_texture;
  void Activate()
  {
    if (m_texture) {
      m_texture->Activate(Unit);
    } else {
      Texture::Deactivate(Unit);
    }
  }
};

} // namespace
} // namespace
