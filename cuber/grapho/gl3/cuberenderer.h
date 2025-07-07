#pragma once
#include "vao.h"
#include <assert.h>
#include <functional>

namespace grapho {
namespace gl3 {

class CubeRenderer
{
  std::shared_ptr<grapho::gl3::Vao> m_cube;
  uint32_t m_cubeDrawCount = 0;
  uint32_t m_mode = 0;

  // pbr: set up projection and view matrices for capturing data onto the 6
  // cubemap face directions
  // ---------------------------------------------------------------------------
  DirectX::XMFLOAT4X4 m_captureProjection;
  DirectX::XMFLOAT4X4 m_captureViews[6];

public:
  CubeRenderer();

  ~CubeRenderer() {}

  using CallbackFunc =
    std::function<void(const DirectX::XMFLOAT4X4& projection, const DirectX::XMFLOAT4X4& view)>;
  void Render(int size,
              uint32_t dst,
              const CallbackFunc& callback,
              int mipLevel = 0) const;
};

}
}
