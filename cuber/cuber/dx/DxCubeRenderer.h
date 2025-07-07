#pragma once
#include <cuber/mesh.h>
#include <memory>
#include <span>
#include <winrt/base.h>

struct ID3D11Device;

namespace cuber {
namespace dx11 {

class DxCubeRenderer
{
  struct DxCubeRendererImpl* m_impl = nullptr;

public:
  Pallete Pallete = {};
  DxCubeRenderer(const DxCubeRenderer&) = delete;
  DxCubeRenderer& operator=(const DxCubeRenderer&) = delete;

  DxCubeRenderer(const winrt::com_ptr<ID3D11Device>& device);
  ~DxCubeRenderer();
  void UploadPallete();
  void Render(const float projection[16],
              const float view[16],
              const Instance* data,
              uint32_t instanceCount);
};

}
} // namespace cuber::dx11
