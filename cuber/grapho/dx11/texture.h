#pragma once
#include <d3d11.h>
#include <memory>
#include <stdint.h>
#include <winrt/base.h>

namespace grapho {
namespace dx11 {

struct Texture
{
  winrt::com_ptr<ID3D11Texture2D> Texture2D;
  winrt::com_ptr<ID3D11ShaderResourceView> Srv;
  winrt::com_ptr<ID3D11SamplerState> Sampler;

  static std::shared_ptr<Texture> Create(
    const winrt::com_ptr<ID3D11Device>& device,
    uint32_t width,
    uint32_t height,
    const uint8_t* pixels = nullptr,
    bool shared = false)
  {
    UINT miscFlags = 0;
    if (shared) {
      miscFlags |= D3D11_RESOURCE_MISC_SHARED;
    }
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (shared) {
      bindFlags |= D3D11_BIND_RENDER_TARGET;
    }
    UINT cpuAccessFlags = 0;
    if (shared) {
      cpuAccessFlags |= D3D11_CPU_ACCESS_WRITE;
    }
    D3D11_TEXTURE2D_DESC desc = {
      .Width = width,
      .Height = height,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .SampleDesc{ .Count = 1 },
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = bindFlags,
      // .CPUAccessFlags = cpuAccessFlags,
      .MiscFlags = miscFlags,
    };
    D3D11_SUBRESOURCE_DATA initData{
      .pSysMem = pixels,
      .SysMemPitch = 8,
      .SysMemSlicePitch = 16,
    };
    auto ptr = std::make_shared<Texture>();
    auto hr = device->CreateTexture2D(&desc, &initData, ptr->Texture2D.put());
    if (FAILED(hr)) {
      return {};
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {
      .Format = desc.Format,
      .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
      .Texture2D{
        .MostDetailedMip = 0,
        .MipLevels = desc.MipLevels,
      },
    };
    hr = device->CreateShaderResourceView(
      ptr->Texture2D.get(), &viewDesc, ptr->Srv.put());
    if (FAILED(hr)) {
      return {};
    }

    D3D11_SAMPLER_DESC sampler_desc = {
      .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
      .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
      .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
      .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
    };
    hr = device->CreateSamplerState(&sampler_desc, ptr->Sampler.put());
    if (FAILED(hr)) {
      return {};
    }

    return ptr;
  }

  void Bind(const winrt::com_ptr<ID3D11DeviceContext>& context, uint32_t slot)
  {
    ID3D11ShaderResourceView* srvs[] = {
      Srv.get(),
    };
    context->PSSetShaderResources(slot, std::size(srvs), srvs);

    ID3D11SamplerState* samplers[] = {
      Sampler.get(),
    };
    context->PSSetSamplers(slot, std::size(samplers), samplers);
  }
};

}
}
