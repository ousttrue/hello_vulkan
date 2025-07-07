#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <memory>
#include <winrt/base.h>

namespace grapho {
namespace dx11 {

inline winrt::com_ptr<ID3D11Device>
CreateDevice(const winrt::com_ptr<IDXGIAdapter>& adapter = {})
{
  D3D_DRIVER_TYPE dtype = D3D_DRIVER_TYPE_HARDWARE;
  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // for D2D
#ifdef _DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
  UINT numFeatureLevels = std::size(featureLevels);
  UINT sdkVersion = D3D11_SDK_VERSION;
  D3D_FEATURE_LEVEL validFeatureLevel;
  winrt::com_ptr<ID3D11Device> device;
  HRESULT hr = D3D11CreateDevice(adapter.get(),
                                 dtype,
                                 nullptr,
                                 flags,
                                 featureLevels,
                                 numFeatureLevels,
                                 sdkVersion,
                                 device.put(),
                                 &validFeatureLevel,
                                 nullptr);
  if (FAILED(hr)) {
    return nullptr;
  }
  if (validFeatureLevel != featureLevels[0]) {
    return nullptr;
  }

  return device;
}

inline winrt::com_ptr<IDXGIFactory2>
GetFactory(const winrt::com_ptr<IDXGIDevice1>& pDXGIDevice)
{
  winrt::com_ptr<IDXGIAdapter> pDXGIAdapter;
  auto hr = pDXGIDevice->GetAdapter(pDXGIAdapter.put());
  if (FAILED(hr)) {
    return nullptr;
  }

  winrt::com_ptr<IDXGIFactory2> pDXGIFactory;
  hr = pDXGIAdapter->GetParent(IID_PPV_ARGS(&pDXGIFactory));
  if (FAILED(hr)) {
    return nullptr;
  }

  return pDXGIFactory;
}

inline winrt::com_ptr<IDXGISwapChain>
CreateSwapchain(const winrt::com_ptr<ID3D11Device>& device, HWND hwnd)
{
  winrt::com_ptr<IDXGIDevice1> pDXGIDevice;
  device.as(pDXGIDevice);
  if (!pDXGIDevice) {
    return nullptr;
  }

  auto pDXGIFactory = GetFactory(pDXGIDevice);
  if (!pDXGIFactory) {
    return nullptr;
  }

  DXGI_SWAP_CHAIN_FULLSCREEN_DESC sfd = {
    .Windowed = TRUE,
  };

  DXGI_SWAP_CHAIN_DESC1 sd = {
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc={
      .Count = 1,
      .Quality = 0,
    },
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
#if 1
    .BufferCount = 2,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
#else
    .BufferCount = 1,
    .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
#endif
    // .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
    // .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
  };

  winrt::com_ptr<IDXGISwapChain1> pSwapChain;
  auto hr = pDXGIFactory->CreateSwapChainForHwnd(
    pDXGIDevice.get(), hwnd, &sd, &sfd, nullptr, pSwapChain.put());
  if (FAILED(hr)) {
    return nullptr;
  }

#if 1
  pDXGIFactory->MakeWindowAssociation(
    hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
#endif

  return pSwapChain;
}
}
}
