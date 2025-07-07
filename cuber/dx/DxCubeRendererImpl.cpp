#include <DirectXMath.h>

#include "DxCubeRendererImpl.h"
#include "cuber_shader.h"
#include "cuber_stereo_shader.h"
#include <grapho/dx11/buffer.h>
#include <grapho/dx11/drawable.h>
#include <grapho/dx11/shader.h>

namespace cuber {
namespace dx11 {

DxCubeRendererImpl::DxCubeRendererImpl(
  const winrt::com_ptr<ID3D11Device>& device,
  bool stereo)
  : device_(device)
  , stereo_(stereo)
{
  device_->GetImmediateContext(context_.put());

  std::string_view shader = stereo_ ? STEREO_SHADER : SHADER;
  auto vs = grapho::dx11::CompileShader(shader, "vs_main", "vs_5_0");
  if (!vs) {
    OutputDebugStringA(vs.error().c_str());
  }
  auto hr = device_->CreateVertexShader((*vs)->GetBufferPointer(),
                                        (*vs)->GetBufferSize(),
                                        NULL,
                                        vertex_shader_.put());
  assert(SUCCEEDED(hr));

  auto ps = grapho::dx11::CompileShader(shader, "ps_main", "ps_5_0");
  if (!ps) {
    OutputDebugStringA(ps.error().c_str());
  }
  hr = device_->CreatePixelShader((*ps)->GetBufferPointer(),
                                  (*ps)->GetBufferSize(),
                                  NULL,
                                  pixel_shader_.put());
  assert(SUCCEEDED(hr));

  auto [vertices, indices, layouts] = Cube(false, stereo_);

  auto vertex_buffer = grapho::dx11::CreateVertexBuffer(device_, vertices);
  instance_buffer_ = grapho::dx11::CreateVertexBuffer(
    device_, sizeof(Instance) * 65535, nullptr);
  auto index_buffer = grapho::dx11::CreateIndexBuffer(device_, indices);

  grapho::dx11::VertexSlot slots[]{
    { .VertexBuffer = vertex_buffer, .Stride = sizeof(Vertex) },
    { .VertexBuffer = instance_buffer_, .Stride = sizeof(Instance) },
  };

  drawable_ =
    grapho::dx11::Drawable::Create(device_, *vs, layouts, slots, index_buffer);
  assert(drawable_);

  constant_buffer_ = grapho::dx11::CreateConstantBuffer(
    device_, sizeof(DirectX::XMFLOAT4X4) * (stereo_ ? 2 : 1), nullptr);
  assert(constant_buffer_);

  pallete_buffer_ =
    grapho::dx11::CreateConstantBuffer(device_, sizeof(Pallete), nullptr);
  assert(pallete_buffer_);
}

void
DxCubeRendererImpl::UploadPallete(const Pallete& pallete)
{
  context_->UpdateSubresource(pallete_buffer_.get(), 0, NULL, &pallete, 0, 0);
}

void
DxCubeRendererImpl::UploadView(const float projection[16],
                               const float view[16],
                               const float rightProjection[16],
                               const float rightView[16])
{
  // 2
  DirectX::XMFLOAT4X4 vp[2];
  {
    // left
    auto v = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)view);
    auto p = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)projection);
    DirectX::XMStoreFloat4x4(&vp[0], v * p);
  }
  if (stereo_ && rightProjection && rightView) {
    // right
    auto v = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)rightView);
    auto p =
      DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)rightProjection);
    DirectX::XMStoreFloat4x4(&vp[1], v * p);
  }
  context_->UpdateSubresource(constant_buffer_.get(), 0, NULL, vp, 0, 0);
}

void
DxCubeRendererImpl::UploadInstance(const void* data, uint32_t instanceCount)
{
  if (instanceCount == 0) {
    return;
  }

  D3D11_BOX box{
    .left = 0,
    .top = 0,
    .front = 0,
    .right = static_cast<uint32_t>(sizeof(Instance) * instanceCount),
    .bottom = 1,
    .back = 1,
  };
  context_->UpdateSubresource(
    instance_buffer_.get(), 0, &box, data, sizeof(Instance), 0);
}

void
DxCubeRendererImpl::Render(uint32_t instanceCount)
{
  context_->VSSetShader(vertex_shader_.get(), NULL, 0);
  context_->PSSetShader(pixel_shader_.get(), NULL, 0);
  ID3D11Buffer* vscb[]{ constant_buffer_.get() };
  context_->VSSetConstantBuffers(0, 1, vscb);
  ID3D11Buffer* pscb[]{ pallete_buffer_.get() };
  context_->PSSetConstantBuffers(1, 1, pscb);

  // 2
  drawable_->DrawInstance(
    context_, CUBE_INDEX_COUNT, instanceCount * (stereo_ ? 2 : 1));
}

}
}
