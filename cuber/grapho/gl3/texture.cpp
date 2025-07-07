// #include <Windows.h>
#include <GL/glew.h>

#include "texture.h"
#include <assert.h>

namespace grapho {
namespace gl3 {

std::optional<uint32_t>
GLImageFormat(PixelFormat format, ColorSpace colorspace)
{
  if (colorspace == ColorSpace::Linear) {
    switch (format) {
      case PixelFormat::f32_RGB:
        return GL_RGB32F;
      case PixelFormat::f16_RGB:
        return GL_RGB16F;
      case PixelFormat::u8_RGBA:
        return GL_RGBA;
      case PixelFormat::u8_RGB:
        return GL_RGB;
      case PixelFormat::u8_R:
        return GL_RED;
      default:
        break;
    }
  } else {
    switch (format) {
      case PixelFormat::u8_RGBA:
        return GL_SRGB8_ALPHA8;
      case PixelFormat::u8_RGB:
        return GL_SRGB8;
      default:
        break;
    }
  }

  assert(false);
  return std::nullopt;
}

uint32_t
GLInternalFormat(PixelFormat format)
{
  switch (format) {
    case PixelFormat::u8_RGBA:
      return GL_RGBA;

    case PixelFormat::u8_R:
      return GL_RED;

    default:
      break;
  }
  return GL_RGB;
}

Texture::Texture()
{
  glGenTextures(1, &m_handle);
}

Texture::~Texture()
{
  glDeleteTextures(1, &m_handle);
}

void
Texture::Bind() const
{
  glBindTexture(GL_TEXTURE_2D, m_handle);
}

void
Texture::Unbind() const
{
  glBindTexture(GL_TEXTURE_2D, 0);
}

void
Texture::Activate(uint32_t unit) const
{
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, m_handle);
}

void
Texture::Deactivate(uint32_t unit)
{
  glDisable(GL_TEXTURE0 + unit);
}

void
Texture::Upload(const Image& data, bool useFloat)
{
  SamplingLinear();
  WrapClamp();
  Bind();
  if (auto format = GLImageFormat(data.Format, data.ColorSpace)) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 *format,
                 data.Width,
                 data.Height,
                 0,
                 GLInternalFormat(data.Format),
                 useFloat ? GL_FLOAT : GL_UNSIGNED_BYTE,
                 data.Pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &m_width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &m_height);
  }
  Unbind();
}

void
Texture::WrapClamp()
{
  Bind();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  Unbind();
}

void
Texture::WrapRepeat()
{
  Bind();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  Unbind();
}

void
Texture::SamplingPoint()
{
  Bind();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  Unbind();
}

void
Texture::SamplingLinear(bool mip)
{
  Bind();
  if (mip) {
    glTexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  Unbind();
}

} // namespace
} // namespace
