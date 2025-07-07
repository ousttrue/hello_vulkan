#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <functional>
#include <optional>
#include <string_view>
#include <winrt/base.h>

namespace grapho {
namespace dx11 {

inline std::optional<winrt::com_ptr<ID3DBlob>>
CompileShader(std::string_view src,
              const char* entry,
              const char* target,
              const std::function<void(const char*)>& logger)
{
  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
  flags |= D3DCOMPILE_DEBUG; // add more debug output
#endif
  winrt::com_ptr<ID3DBlob> vs_blob_ptr;
  winrt::com_ptr<ID3DBlob> error_blob;
  auto hr = D3DCompile(src.data(),
                       src.size(),
                       "shaders.hlsl",
                       nullptr,
                       D3D_COMPILE_STANDARD_FILE_INCLUDE,
                       entry,
                       target,
                       flags,
                       0,
                       vs_blob_ptr.put(),
                       error_blob.put());
  if (FAILED(hr)) {
    if (error_blob) {
      if (logger) {
        logger((const char*)error_blob->GetBufferPointer());
      }
      return {};
    }

    if (logger) {
      logger("D3DCompile");
    }
    return {};
  }
  return vs_blob_ptr;
}

struct Shader
{
  winrt::com_ptr<ID3D11VertexShader> VS;
  winrt::com_ptr<ID3D11PixelShader> PS;

  static std::
    tuple<std::shared_ptr<Shader>, winrt::com_ptr<ID3DBlob>, std::string>
    Create(const winrt::com_ptr<ID3D11Device>& device,
           std::string_view vs_src,
           const char* vs_entry,
           const char* vs_version,
           std::string_view ps_src,
           const char* ps_entry,
           const char* ps_version)
  {
    auto ptr = std::make_shared<Shader>();

    std::string error;
    auto vs_compiled = grapho::dx11::CompileShader(
      vs_src, vs_entry, vs_version, [&error](const char* msg) { error = msg; });
    if (!vs_compiled) {
      // std::cout << vs_compiled.error() << std::endl;
      return { {}, {}, error };
    }
    auto hr = device->CreateVertexShader((*vs_compiled)->GetBufferPointer(),
                                         (*vs_compiled)->GetBufferSize(),
                                         NULL,
                                         ptr->VS.put());
    if (FAILED(hr)) {
      return { {}, {}, "CreateVertexShader" };
    }

    auto ps_compiled = grapho::dx11::CompileShader(
      ps_src, ps_entry, ps_version, [&error](const char* msg) { error = msg; });
    if (!ps_compiled) {
      // std::cout << ps_compiled.error() << std::endl;
      return { {}, {}, error };
    }
    hr = device->CreatePixelShader((*ps_compiled)->GetBufferPointer(),
                                   (*ps_compiled)->GetBufferSize(),
                                   NULL,
                                   ptr->PS.put());
    if (FAILED(hr)) {
      return { {}, {}, "CreatePixelShader" };
    }

    return { ptr, *vs_compiled, {} };
  }

  void Bind(const winrt::com_ptr<ID3D11DeviceContext>& context)
  {
    context->VSSetShader(VS.get(), NULL, 0);
    context->PSSetShader(PS.get(), NULL, 0);
  }
};

}
} // namespace cuber
