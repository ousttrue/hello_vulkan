#include <DirectXMath.h>

#include "DxCubeRendererImpl.h"
#include <cuber/dx/DxCubeRenderer.h>

namespace cuber::dx11 {

DxCubeRenderer::DxCubeRenderer(const winrt::com_ptr<ID3D11Device>& device)
  : m_impl(new DxCubeRendererImpl(device, false))
{
  m_impl->UploadPallete(Pallete);
}

DxCubeRenderer::~DxCubeRenderer()
{
  delete m_impl;
}

void
DxCubeRenderer::UploadPallete()
{
  m_impl->UploadPallete(Pallete);
}

void
DxCubeRenderer::Render(const float projection[16],
                       const float view[16],
                       const Instance* data,
                       uint32_t instanceCount)
{
  m_impl->UploadView(projection, view, nullptr, nullptr);
  m_impl->UploadInstance(data, instanceCount);
  m_impl->Render(instanceCount);
}

}
