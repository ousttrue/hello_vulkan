#pragma once
#include "cuber/mesh.h"
#include <DirectXMath.h>
#include <array>
#include <memory>
#include <span>

namespace grapho {
namespace gl3 {
struct Vao;
class ShaderProgram;
class Vbo;
struct Ubo;
}
}

namespace cuber {
namespace gl3 {

class GlCubeRenderer
{
  std::shared_ptr<grapho::gl3::Vao> m_vao;
  std::shared_ptr<grapho::gl3::ShaderProgram> m_shader;
  std::shared_ptr<grapho::gl3::Vbo> m_instance_vbo;

  std::shared_ptr<grapho::gl3::Ubo> m_ubo;

public:
  Pallete Pallete = {};
  GlCubeRenderer(const GlCubeRenderer&) = delete;
  GlCubeRenderer& operator=(const GlCubeRenderer&) = delete;
  GlCubeRenderer();
  ~GlCubeRenderer();
  void UploadPallete();
  void Render(const float projection[16],
              const float view[16],
              const Instance* data,
              uint32_t instanceCount);
};

}
}
