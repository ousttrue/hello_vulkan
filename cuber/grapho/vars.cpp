#include <DirectXMath.h>

#include "vars.h"

namespace grapho {

DirectX::XMFLOAT3X3
LocalVars::normalMatrix3() const
{
  DirectX::XMFLOAT3X3 m;
  DirectX::XMStoreFloat3x3(
    (DirectX::XMFLOAT3X3*)&m,
    DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)&normalMatrix));
  return m;
}

DirectX::XMFLOAT3X3
LocalVars::uvTransform() const
{
  DirectX::XMFLOAT3X3 m;
  DirectX::XMStoreFloat3x3((DirectX::XMFLOAT3X3*)&m,
                           DirectX::XMMatrixIdentity());
  return m;
}

void
LocalVars::CalcNormalMatrix()
{
  auto m = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)&model);
  DirectX::XMVECTOR det;
  auto ti = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, m));
  // DirectX::XMFLOAT3X3 mat3;
  DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&normalMatrix, ti);
  // normalMatrix._41 = 0;
  // normalMatrix._42 = 0;
  // normalMatrix._43 = 0;
  // return mat3;
}

} // namespace
