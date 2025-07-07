#pragma once
#include "../image.h"
#include "texture.h"
#include <stdint.h>

namespace grapho {
namespace gl3 {

class Cubemap
{
  uint32_t m_handle;
  int m_width = 0;
  int m_height = 0;

public:
  Cubemap() { glGenTextures(1, &m_handle); }
  ~Cubemap() { glDeleteTextures(1, &m_handle); }
  uint32_t Handle() const { return m_handle; }
  static std::shared_ptr<Cubemap> Create(const Image& data,
                                         bool useFloat = false)
  {
    auto ptr = std::shared_ptr<Cubemap>(new Cubemap());
    ptr->m_width = data.Width;
    ptr->m_height = data.Height;
    ptr->Bind();
    if (auto format = GLImageFormat(data.Format, data.ColorSpace)) {
      for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                     0,
                     *format,
                     data.Width,
                     data.Height,
                     0,
                     GLInternalFormat(data.Format),
                     useFloat ? GL_FLOAT : GL_UNSIGNED_BYTE,
                     nullptr);
      }
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
    ptr->UnBind();
    ptr->SamplingLinear();

    return ptr;
  }

  void GenerateMipmap()
  {
    // then let OpenGL generate mipmaps from first mip face (combatting visible
    // dots artifact)
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
  }

  void Bind() { glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle); }
  void UnBind() { glBindTexture(GL_TEXTURE_CUBE_MAP, 0); }

  void SamplingLinear(bool mip = false)
  {
    Bind();
    if (mip) {
      // enable pre-filter mipmap sampling (combatting visible dots artifact)
      glTexParameteri(
        GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    UnBind();
  }

  void Activate(uint32_t unit)
  {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle);
  }
};

}
}
