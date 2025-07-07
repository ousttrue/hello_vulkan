#include <DirectXMath.h>

#include "DxCubeRendererImpl.h"
#include <cuber/dx/DxCubeStereoRenderer.h>

namespace cuber {
namespace dx11 {

DxCubeStereoRenderer::DxCubeStereoRenderer(
  const winrt::com_ptr<ID3D11Device>& device)
  : impl_(new DxCubeRendererImpl(device, true))
{
  impl_->UploadPallete(Pallete);
}
DxCubeStereoRenderer::~DxCubeStereoRenderer()
{
  delete impl_;
}

void
DxCubeStereoRenderer::UploadPallete()
{
  impl_->UploadPallete(Pallete);
}

void
DxCubeStereoRenderer::Render(const float projection[16],
                             const float view[16],
                             const float rightProjection[16],
                             const float rightView[16],
                             const Instance* data,
                             uint32_t instanceCount)
{
  impl_->UploadView(projection, view, rightProjection, rightView);
  impl_->UploadInstance(data, instanceCount);
  impl_->Render(instanceCount);
}

}
}
