#pragma once
#include "vertexlayout.h"

namespace grapho {

struct WorldVars
{
  DirectX::XMFLOAT4 lightPositions[4];
  DirectX::XMFLOAT4 lightColors[4];
  DirectX::XMFLOAT4 camPos;
};

struct LocalVars
{
  DirectX::XMFLOAT4X4 model;
  DirectX::XMFLOAT4 color;
  DirectX::XMFLOAT4 cutoff;
  DirectX::XMFLOAT4X4 normalMatrix;
  DirectX::XMFLOAT3 emissiveColor;
  DirectX::XMFLOAT3X3 normalMatrix3() const;
  DirectX::XMFLOAT3X3 uvTransform() const;
  void CalcNormalMatrix();
};

}
