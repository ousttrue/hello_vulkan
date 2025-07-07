#include "cuberenderer.h"
#include <DirectXMath.h>
#include <GL/glew.h>
#include <grapho/mesh.h>
#include <grapho/gl3/fbo.h>

namespace grapho {
namespace gl3 {

CubeRenderer::CubeRenderer()
{
  auto cube = grapho::mesh::Cube();
  m_cube = grapho::gl3::Vao::Create(cube);
  m_cubeDrawCount = cube->DrawCount();
  m_mode = *grapho::gl3::GLMode(cube->Mode);

  DirectX::XMStoreFloat4x4(
    (DirectX::XMFLOAT4X4*)&m_captureProjection,
    DirectX::XMMatrixPerspectiveFovRH(
      DirectX::XMConvertToRadians(90.0f), 1.0f, 0.1f, 10.0f));

  DirectX::XMStoreFloat4x4(
    (DirectX::XMFLOAT4X4*)&m_captureViews[0],
    DirectX::XMMatrixLookAtRH(DirectX::XMVectorZero(),
                              DirectX::XMVectorSet(1.0f, 0, 0, 0),
                              DirectX::XMVectorSet(0, -1.0f, 0, 0)));
  DirectX::XMStoreFloat4x4(
    (DirectX::XMFLOAT4X4*)&m_captureViews[1],
    DirectX::XMMatrixLookAtRH(DirectX::XMVectorZero(),
                              DirectX::XMVectorSet(-1.0f, 0, 0, 0),
                              DirectX::XMVectorSet(0, -1.0f, 0, 0)));
  DirectX::XMStoreFloat4x4(
    (DirectX::XMFLOAT4X4*)&m_captureViews[2],
    DirectX::XMMatrixLookAtRH(DirectX::XMVectorZero(),
                              DirectX::XMVectorSet(0, 1.0f, 0, 0),
                              DirectX::XMVectorSet(0, 0, 1.0f, 0)));
  DirectX::XMStoreFloat4x4(
    (DirectX::XMFLOAT4X4*)&m_captureViews[3],
    DirectX::XMMatrixLookAtRH(DirectX::XMVectorZero(),
                              DirectX::XMVectorSet(0, -1.0f, 0, 0),
                              DirectX::XMVectorSet(0, 0, -1.0f, 0)));
  DirectX::XMStoreFloat4x4(
    (DirectX::XMFLOAT4X4*)&m_captureViews[4],
    DirectX::XMMatrixLookAtRH(DirectX::XMVectorZero(),
                              DirectX::XMVectorSet(0, 0, 1.0f, 0),
                              DirectX::XMVectorSet(0, -1.0f, 0, 0)));
  DirectX::XMStoreFloat4x4(
    (DirectX::XMFLOAT4X4*)&m_captureViews[5],
    DirectX::XMMatrixLookAtRH(DirectX::XMVectorZero(),
                              DirectX::XMVectorSet(0, 0, -1.0f, 0),
                              DirectX::XMVectorSet(0, -1.0f, 0, 0)));
}

void
CubeRenderer::Render(int size,
                     uint32_t dst,
                     const CallbackFunc& callback,
                     int mipLevel) const
{
  auto fbo = std::make_shared<grapho::gl3::Fbo>();
  grapho::camera::Viewport fboViewport{
    .Width = static_cast<float>(size), .Height = static_cast<float>(size),
    // .Color = { 0, 0, 0, 0 },
  };
  for (unsigned int i = 0; i < 6; ++i) {
    callback(m_captureProjection, m_captureViews[i]);
    fbo->AttachCubeMap(i, dst, mipLevel);
    assert(!TryGetError());
    grapho::gl3::ClearViewport(fboViewport, { .Depth = false });
    assert(!TryGetError());
    m_cube->Draw(m_mode, m_cubeDrawCount);
    assert(!TryGetError());
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace
} // namespace
