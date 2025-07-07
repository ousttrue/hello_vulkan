#pragma once
#include <d3d11.h>
#include <winrt/base.h>

namespace grapho {
namespace dx11 {

inline winrt::com_ptr<ID3D11Buffer>
CreateBuffer(const winrt::com_ptr<ID3D11Device>& device,
             UINT flags,
             uint32_t size,
             const void* data)
{
  D3D11_BUFFER_DESC desc = {
    .ByteWidth = size,
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = flags,
  };
  winrt::com_ptr<ID3D11Buffer> buffer;
  D3D11_SUBRESOURCE_DATA sr_data = {
    .pSysMem = data,
  };
  auto hr =
    device->CreateBuffer(&desc, data ? &sr_data : nullptr, buffer.put());
  if (FAILED(hr)) {
    return {};
  }
  return buffer;
}

inline winrt::com_ptr<ID3D11Buffer>
CreateIndexBuffer(const winrt::com_ptr<ID3D11Device>& device,
                  uint32_t size,
                  const void* data)
{
  return CreateBuffer(device, D3D11_BIND_INDEX_BUFFER, size, data);
}
template<typename T>
inline winrt::com_ptr<ID3D11Buffer>
CreateIndexBuffer(const winrt::com_ptr<ID3D11Device>& device,
                  const std::vector<T>& indices)
{
  return CreateIndexBuffer(device, sizeof(T) * indices.size(), indices.data());
}

inline winrt::com_ptr<ID3D11Buffer>
CreateVertexBuffer(const winrt::com_ptr<ID3D11Device>& device,
                   uint32_t size,
                   const void* data)
{
  return CreateBuffer(device, D3D11_BIND_VERTEX_BUFFER, size, data);
}
template<typename T>
inline winrt::com_ptr<ID3D11Buffer>
CreateVertexBuffer(const winrt::com_ptr<ID3D11Device>& device,
                   const std::vector<T>& vertices)
{
  return CreateVertexBuffer(
    device, sizeof(T) * vertices.size(), vertices.data());
}

inline winrt::com_ptr<ID3D11Buffer>
CreateConstantBuffer(const winrt::com_ptr<ID3D11Device>& device,
                     uint32_t size,
                     const void* data)
{
  return CreateBuffer(device, D3D11_BIND_CONSTANT_BUFFER, size, data);
}
template<typename T>
inline winrt::com_ptr<ID3D11Buffer>
CreateConstantBuffer(const winrt::com_ptr<ID3D11Device>& device, const T& t)
{
  return CreateBuffer(device, D3D11_BIND_CONSTANT_BUFFER, sizeof(T), &t);
}

}
}
