#pragma once
#include <array>
#include <cuber/mesh.h>
#include <memory>
#include <span>
#include <winrt/base.h>

struct ID3D11Device;

namespace cuber::dx11 {

class DxCubeStereoRenderer
{
  struct DxCubeRendererImpl* impl_ = nullptr;

public:
  Pallete Pallete = {};
  DxCubeStereoRenderer(const DxCubeStereoRenderer&) = delete;
  DxCubeStereoRenderer& operator=(const DxCubeStereoRenderer&) = delete;
  DxCubeStereoRenderer(const winrt::com_ptr<ID3D11Device>& device);
  ~DxCubeStereoRenderer();
  void UploadPallete();
  void Render(const float projection[16],
              const float view[16],
              const float rightProjection[16],
              const float rightView[16],
              const Instance* data,
              uint32_t instanceCount);
};

} // namespace cuber
